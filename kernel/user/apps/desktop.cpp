// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/apps/file_manager.hh>
#include <user/apps/task_manager.hh>
#include <user/apps/terminal.hh>
#include <user/gui.hh>
#include <user/syscall.hh>

namespace
{
    constexpr uint32_t TASKBAR_HEIGHT = 48;
    constexpr uint32_t WINDOW_FILE = 1;
    constexpr uint32_t WINDOW_TERMINAL = 2;
    constexpr uint32_t WINDOW_TASK_MANAGER = 3;
    constexpr uint32_t WINDOW_CALCULATOR = 4;
    constexpr uint32_t WINDOW_NOTEPAD = 5;

    struct WindowBounds
    {
        int32_t x;
        int32_t y;
        uint32_t width;
        uint32_t height;
    };

    uint32_t min_u32(uint32_t a, uint32_t b)
    {
        return a < b ? a : b;
    }

    class DesktopSession
    {
    public:
        DesktopSession()
            : screen_{}, file_manager_{}, terminal_{}, task_manager_{},
              app_process_ids_{0, 0, 0, 0, 0, 0}, foreground_window_(0),
              logo_available_(false), file_visible_(false),
              terminal_visible_(false), file_taskbar_(false),
              terminal_taskbar_(false), task_visible_(false), task_taskbar_(false),
              calculator_taskbar_(false), notepad_taskbar_(false),
              about_open_(false), menu_open_(false),
              console_mode_(false), dragging_(false), drag_window_(0), active_(0),
              mouse_x_(0), mouse_y_(0), last_buttons_(0), drag_offset_x_(0),
              drag_offset_y_(0)
        {
            user::graphics_get_info(&screen_);
            mouse_x_ = static_cast<int32_t>(screen_.width / 2);
            mouse_y_ = static_cast<int32_t>(screen_.height / 2);
            file_bounds_ = centered_window(650, 430, 25);
            terminal_bounds_ = centered_window(560, 340, 55);
            task_bounds_ = centered_window(720, 470, 75);
            logo_available_ = load_logo();
        }

        [[noreturn]] void run()
        {
            user::thread_set_name("PCKOS masaustu");
            user::graphics_cursor(static_cast<uint32_t>(mouse_x_),
                                  static_cast<uint32_t>(mouse_y_), true);
            render();
            for (;;)
            {
                user::InputEvent event{};
                if (user::wait_event(&event, 10) <= 0)
                    continue;
                bool desktop_changed = false;
                for (uint32_t id = WINDOW_FILE; id <= WINDOW_NOTEPAD; ++id)
                {
                    if (app_process_ids_[id] == 0 ||
                        process_is_running(app_process_ids_[id]))
                        continue;
                    app_process_ids_[id] = 0;
                    if (id == WINDOW_FILE) file_taskbar_ = false;
                    else if (id == WINDOW_TERMINAL) terminal_taskbar_ = false;
                    else if (id == WINDOW_TASK_MANAGER) task_taskbar_ = false;
                    else if (id == WINDOW_CALCULATOR) calculator_taskbar_ = false;
                    else notepad_taskbar_ = false;
                    desktop_changed = true;
                }
                if (foreground_window_ != 0)
                {
                    const uint32_t process_id = app_process_ids_[foreground_window_];
                    if (process_id != 0 && !user::process_is_minimized(process_id) &&
                        event.type == 3)
                    {
                        user::gui::begin_frame();
                        draw_taskbar();
                        user::gui::end_frame();
                        continue;
                    }
                    foreground_window_ = 0;
                    active_ = 0;
                    last_buttons_ = 0;
                    desktop_changed = true;
                }
                if (desktop_changed)
                    render();
                if (event.type == 1)
                    handle_key(event.value0);
                else if (event.type == 2)
                    handle_mouse(event);
                else if (event.type == 3)
                {
                    if (console_mode_)
                        render_console();
                    else
                    {
                        user::gui::begin_frame();
                        draw_taskbar();
                        user::gui::end_frame();
                    }
                }
            }
        }

    private:
        static void append_text(char *destination, uint32_t capacity,
                                uint32_t *at, const char *value)
        {
            while (*value != '\0' && *at + 1 < capacity)
                destination[(*at)++] = *value++;
            destination[*at] = '\0';
        }

        static void append_number(char *destination, uint32_t capacity,
                                  uint32_t *at, uint32_t value)
        {
            char reverse[10];
            uint32_t count = 0;
            do
            {
                reverse[count++] = static_cast<char>('0' + value % 10);
                value /= 10;
            } while (value != 0 && count < sizeof(reverse));
            while (count != 0 && *at + 1 < capacity)
                destination[(*at)++] = reverse[--count];
            destination[*at] = '\0';
        }

        bool process_is_running(uint32_t process_id)
        {
            const uint32_t count = user::process_count();
            for (uint32_t index = 0; index < count; ++index)
            {
                user::ProcessInfo info{};
                if (user::process_get_info(index, &info) == 0 &&
                    info.id == process_id)
                    return true;
            }
            return false;
        }

        bool restore_app(uint32_t id)
        {
            if (id < WINDOW_FILE || id > WINDOW_NOTEPAD ||
                app_process_ids_[id] == 0)
                return false;
            active_ = id;
            foreground_window_ = id;
            menu_open_ = false;
            about_open_ = false;
            render();
            if (user::process_focus(app_process_ids_[id]) < 0)
            {
                foreground_window_ = 0;
                active_ = 0;
                return false;
            }
            return true;
        }

        static uint16_t read_u16(const uint8_t *data)
        {
            return static_cast<uint16_t>(data[0] |
                (static_cast<uint16_t>(data[1]) << 8));
        }

        static uint32_t read_u32(const uint8_t *data)
        {
            return static_cast<uint32_t>(data[0]) |
                (static_cast<uint32_t>(data[1]) << 8) |
                (static_cast<uint32_t>(data[2]) << 16) |
                (static_cast<uint32_t>(data[3]) << 24);
        }

        static bool read_exact(int32_t descriptor, uint8_t *buffer,
                               uint32_t length)
        {
            uint32_t offset = 0;
            while (offset < length)
            {
                const int32_t count = user::read(descriptor, buffer + offset,
                                                  length - offset);
                if (count <= 0)
                    return false;
                offset += static_cast<uint32_t>(count);
            }
            return true;
        }

        bool load_logo()
        {
            const int32_t descriptor = user::open("/system/logo.bmp");
            if (descriptor < 0)
                return false;

            uint8_t header[138];
            if (!read_exact(descriptor, header, sizeof(header)) ||
                header[0] != 'B' || header[1] != 'M')
            {
                user::close(descriptor);
                return false;
            }
            const uint32_t pixel_offset = read_u32(header + 10);
            const int32_t image_width = static_cast<int32_t>(read_u32(header + 18));
            const int32_t signed_height = static_cast<int32_t>(read_u32(header + 22));
            const uint16_t bits_per_pixel = read_u16(header + 28);
            const uint32_t compression = read_u32(header + 30);
            if (image_width <= 0 || image_width > 512 || signed_height == 0 ||
                signed_height < -512 || signed_height > 512 ||
                bits_per_pixel != 32 || compression != 3 ||
                pixel_offset < sizeof(header) ||
                read_u32(header + 54) != 0x00FF0000 ||
                read_u32(header + 58) != 0x0000FF00 ||
                read_u32(header + 62) != 0x000000FF ||
                read_u32(header + 66) != 0xFF000000)
            {
                user::close(descriptor);
                return false;
            }

            uint32_t skip = pixel_offset - sizeof(header);
            uint8_t discard[128];
            while (skip != 0)
            {
                const uint32_t count = skip < sizeof(discard) ? skip : sizeof(discard);
                if (!read_exact(descriptor, discard, count))
                {
                    user::close(descriptor);
                    return false;
                }
                skip -= count;
            }

            const uint32_t image_height = signed_height < 0
                ? static_cast<uint32_t>(-signed_height)
                : static_cast<uint32_t>(signed_height);
            const uint32_t stride = static_cast<uint32_t>(image_width) * 4;
            uint8_t row[512 * 4];
            for (uint32_t source_y = 0; source_y < image_height; ++source_y)
            {
                if (!read_exact(descriptor, row, stride))
                {
                    user::close(descriptor);
                    return false;
                }
                const uint32_t top_y = signed_height > 0
                    ? image_height - 1 - source_y : source_y;
                const uint32_t first_y = (top_y * 128 + image_height - 1) / image_height;
                const uint32_t last_y = ((top_y + 1) * 128 + image_height - 1) / image_height;
                for (uint32_t target_y = first_y; target_y < last_y; ++target_y)
                for (uint32_t x = 0; x < 128; ++x)
                {
                    const uint32_t source_x = x * static_cast<uint32_t>(image_width) / 128;
                    const uint8_t *pixel = row + source_x * 4;
                    logo_pixels_[target_y * 128 + x] =
                        (static_cast<uint32_t>(pixel[3]) << 24) |
                        (static_cast<uint32_t>(pixel[2]) << 16) |
                        (static_cast<uint32_t>(pixel[1]) << 8) | pixel[0];
                }
            }
            user::close(descriptor);
            return true;
        }

        WindowBounds centered_window(uint32_t wanted_width, uint32_t wanted_height,
                                     uint32_t x_offset)
        {
            const uint32_t width = min_u32(wanted_width,
                screen_.width > 28 ? screen_.width - 28 : screen_.width);
            const uint32_t available_height = screen_.height > TASKBAR_HEIGHT + 12
                ? screen_.height - TASKBAR_HEIGHT - 12 : screen_.height;
            const uint32_t height = min_u32(wanted_height, available_height);
            const int32_t centered_x = static_cast<int32_t>(
                screen_.width > width ? (screen_.width - width) / 2 : 0);
            const int32_t centered_y = static_cast<int32_t>(
                available_height > height ? (available_height - height) / 2 : 0);
            WindowBounds result{centered_x + static_cast<int32_t>(x_offset),
                                centered_y + 24, width, height};
            clamp_window(&result);
            return result;
        }

        void clamp_window(WindowBounds *window)
        {
            const int32_t max_x = screen_.width > window->width
                ? static_cast<int32_t>(screen_.width - window->width) : 0;
            const uint32_t usable_height = screen_.height > TASKBAR_HEIGHT
                ? screen_.height - TASKBAR_HEIGHT : screen_.height;
            const int32_t max_y = usable_height > window->height
                ? static_cast<int32_t>(usable_height - window->height) : 0;
            if (window->x < 0) window->x = 0;
            if (window->y < 0) window->y = 0;
            if (window->x > max_x) window->x = max_x;
            if (window->y > max_y) window->y = max_y;
        }

        WindowBounds &bounds(uint32_t id)
        {
            if (id == WINDOW_FILE) return file_bounds_;
            if (id == WINDOW_TERMINAL) return terminal_bounds_;
            return task_bounds_;
        }

        bool visible(uint32_t id) const
        {
            if (id == WINDOW_FILE) return file_visible_;
            if (id == WINDOW_TERMINAL) return terminal_visible_;
            return task_visible_;
        }

        uint32_t fallback_window() const
        {
            if (file_visible_) return WINDOW_FILE;
            if (terminal_visible_) return WINDOW_TERMINAL;
            if (task_visible_) return WINDOW_TASK_MANAGER;
            return 0;
        }

        void open_window(uint32_t id)
        {
            if (id < WINDOW_FILE || id > WINDOW_NOTEPAD)
                return;
            if (app_process_ids_[id] != 0)
            {
                if (process_is_running(app_process_ids_[id]))
                {
                    (void)restore_app(id);
                    return;
                }
                app_process_ids_[id] = 0;
            }
            const char *path = id == WINDOW_FILE ? "/apps/file_manager.bin"
                : id == WINDOW_TERMINAL ? "/apps/terminal.bin"
                : id == WINDOW_TASK_MANAGER ? "/apps/task_manager.bin"
                : id == WINDOW_CALCULATOR ? "/apps/calculator.bin"
                : "/apps/notepad.bin";
            const char *name = id == WINDOW_FILE ? "Dosya Yoneticisi"
                : id == WINDOW_TERMINAL ? "Terminal"
                : id == WINDOW_TASK_MANAGER ? "Gorev Yoneticisi"
                : id == WINDOW_CALCULATOR ? "Hesap Makinesi"
                : "Not Defteri";
            file_visible_ = terminal_visible_ = task_visible_ = false;
            if (id == WINDOW_FILE) file_taskbar_ = true;
            else if (id == WINDOW_TERMINAL) terminal_taskbar_ = true;
            else if (id == WINDOW_TASK_MANAGER) task_taskbar_ = true;
            else if (id == WINDOW_CALCULATOR) calculator_taskbar_ = true;
            else notepad_taskbar_ = true;
            active_ = id;
            foreground_window_ = id;
            menu_open_ = false;
            about_open_ = false;
            render();
            const int32_t process_id = user::process_spawn(path, name);
            if (process_id < 0)
            {
                foreground_window_ = 0;
                active_ = 0;
                if (id == WINDOW_FILE) file_taskbar_ = false;
                else if (id == WINDOW_TERMINAL) terminal_taskbar_ = false;
                else if (id == WINDOW_TASK_MANAGER) task_taskbar_ = false;
                else if (id == WINDOW_CALCULATOR) calculator_taskbar_ = false;
                else notepad_taskbar_ = false;
                render();
                return;
            }
            app_process_ids_[id] = static_cast<uint32_t>(process_id);
        }

        void draw_icon_label(uint32_t x, uint32_t y, uint32_t kind,
                             const char *label)
        {
            if (kind == 0)
                draw_logo_small(x + 2, y + 1, 40);
            else
                user::gui::icon(x, y, kind);
            user::gui::text(x - 1, y + 43, label, user::gui::COLOR_BLACK);
            user::gui::text(x - 2, y + 42, label, user::gui::COLOR_WHITE);
        }

        void draw_logo_small(uint32_t x, uint32_t y, uint32_t size)
        {
            if (!logo_available_ || size == 0 || size > 40)
            {
                user::gui::pckos_mark(x, y, size);
                return;
            }
            uint32_t pixels[40 * 40];
            for (uint32_t row = 0; row < size; ++row)
                for (uint32_t column = 0; column < size; ++column)
                    pixels[row * size + column] = logo_pixels_[
                        (row * 128 / size) * 128 + column * 128 / size];
            user::gui::bitmap(x, y, pixels, size, size);
        }

        void draw_taskbar()
        {
            const uint32_t focused_process = user::process_focused_id();
            const uint32_t y = screen_.height > TASKBAR_HEIGHT
                ? screen_.height - TASKBAR_HEIGHT : 0;
            user::gui::frame(0, y, screen_.width, TASKBAR_HEIGHT,
                             user::gui::COLOR_FACE);
            user::gui::frame(5, y + 4, 98, 40, user::gui::COLOR_FACE, menu_open_);
            draw_logo_small(13, y + 12, 24);
            user::gui::text(43, y + 15, "Baslat", user::gui::COLOR_TEXT);

            if (file_taskbar_)
                user::gui::button(110, y + 4, task_button_width(), 40, "Dosyalar",
                                  focused_process == app_process_ids_[WINDOW_FILE]);
            if (terminal_taskbar_)
                user::gui::button(terminal_task_x(), y + 4, task_button_width(), 40, "Terminal",
                                  focused_process == app_process_ids_[WINDOW_TERMINAL]);
            if (task_taskbar_)
                user::gui::button(task_manager_task_x(), y + 4,
                                  task_button_width(), 40, "Gorevler",
                                  focused_process == app_process_ids_[WINDOW_TASK_MANAGER]);
            if (calculator_taskbar_)
                user::gui::button(calculator_task_x(), y + 4,
                                  task_button_width(), 40, "Hesap",
                                  focused_process == app_process_ids_[WINDOW_CALCULATOR]);
            if (notepad_taskbar_)
                user::gui::button(notepad_task_x(), y + 4,
                                  task_button_width(), 40, "Notlar",
                                  focused_process == app_process_ids_[WINDOW_NOTEPAD]);

            draw_clock();
        }

        uint32_t task_button_width() const
        {
            return min_u32(136, screen_.width > 275
                ? (screen_.width - 275) / 5 : 24);
        }

        uint32_t terminal_task_x() const { return 115 + task_button_width(); }
        uint32_t task_manager_task_x() const
        {
            return terminal_task_x() + task_button_width() + 5;
        }
        uint32_t calculator_task_x() const
        {
            return task_manager_task_x() + task_button_width() + 5;
        }
        uint32_t notepad_task_x() const
        {
            return calculator_task_x() + task_button_width() + 5;
        }

        void draw_clock()
        {
            const uint32_t y = screen_.height - TASKBAR_HEIGHT;
            const uint32_t clock_x = screen_.width > 130 ? screen_.width - 126 : 4;
            user::gui::frame(clock_x, y + 4, 120, 40, user::gui::COLOR_FACE, true);
            user::DateTime now{};
            if (user::get_datetime(&now) != 0)
            {
                user::gui::text(clock_x + 28, y + 16, "Saat yok",
                                user::gui::COLOR_TEXT);
                return;
            }
            char clock[9] = {
                static_cast<char>('0' + now.hour / 10),
                static_cast<char>('0' + now.hour % 10), ':',
                static_cast<char>('0' + now.minute / 10),
                static_cast<char>('0' + now.minute % 10), ':',
                static_cast<char>('0' + now.second / 10),
                static_cast<char>('0' + now.second % 10), '\0'
            };
            char date[11] = {
                static_cast<char>('0' + now.day / 10),
                static_cast<char>('0' + now.day % 10), '.',
                static_cast<char>('0' + now.month / 10),
                static_cast<char>('0' + now.month % 10), '.',
                static_cast<char>('0' + now.year / 1000 % 10),
                static_cast<char>('0' + now.year / 100 % 10),
                static_cast<char>('0' + now.year / 10 % 10),
                static_cast<char>('0' + now.year % 10), '\0'
            };
            user::gui::text(clock_x + 28, y + 5, clock, user::gui::COLOR_TEXT);
            user::gui::text(clock_x + 20, y + 24, date, user::gui::COLOR_TEXT);
        }

        void draw_window(uint32_t id)
        {
            WindowBounds &window = bounds(id);
            const bool is_active = active_ == id;
            const char *title = id == WINDOW_FILE ? "PCKOS - Dosya Yonetimi"
                : (id == WINDOW_TERMINAL ? "PCKOS - Terminal"
                                          : "PCKOS - Gorev Yoneticisi");
            user::gui::window(static_cast<uint32_t>(window.x),
                static_cast<uint32_t>(window.y), window.width, window.height,
                title, is_active);
            const uint32_t content_x = static_cast<uint32_t>(window.x) + 7;
            const uint32_t content_y = static_cast<uint32_t>(window.y) + 32;
            const uint32_t content_width = window.width > 14
                ? window.width - 14 : 1;
            const uint32_t content_height = window.height > 40
                ? window.height - 40 : 1;
            user::gui::push_clip(content_x, content_y, content_width, content_height);
            if (id == WINDOW_FILE)
                file_manager_.draw(content_x, content_y, content_width,
                                   content_height);
            else if (id == WINDOW_TERMINAL)
                terminal_.draw(content_x, content_y, content_width,
                               content_height);
            else
                task_manager_.draw(content_x, content_y, content_width,
                                   content_height);
            user::gui::pop_clip();
        }

        void draw_start_menu()
        {
            const uint32_t y = screen_.height > TASKBAR_HEIGHT + 282
                ? screen_.height - TASKBAR_HEIGHT - 282 : 0;
            user::gui::frame(7, y, 226, 278, user::gui::COLOR_FACE);
            user::gui::fill(11, y + 4, 34, 270, user::gui::COLOR_TITLE);
            user::gui::text(24, y + 15, "P\nC\nK\nO\nS", user::gui::COLOR_WHITE);
            user::gui::button(51, y + 8, 174, 34, "Dosyalar");
            user::gui::button(51, y + 46, 174, 34, "Terminal");
            user::gui::button(51, y + 84, 174, 34, "Gorev Yoneticisi");
            user::gui::button(51, y + 122, 174, 34, "Hesap Makinesi");
            user::gui::button(51, y + 160, 174, 34, "Not Defteri");
            user::gui::button(51, y + 198, 174, 34, "PCKOS Hakkinda");
            user::gui::button(51, y + 236, 174, 34, "Konsol");
        }

        void draw_about()
        {
            const uint32_t width = min_u32(660, screen_.width - 20);
            const uint32_t height = min_u32(400, screen_.height - TASKBAR_HEIGHT - 12);
            const uint32_t x = (screen_.width - width) / 2;
            const uint32_t y = (screen_.height - TASKBAR_HEIGHT - height) / 2;
            user::SystemInfo info{};
            (void)user::get_system_info(&info);
            user::GraphicsInfo graphics{};
            (void)user::graphics_get_info(&graphics);

            user::gui::window(x, y, width, height,
                              "PCKOS - Sistem bilgisi ve yetenekler", true);
            user::gui::push_clip(x + 7, y + 32, width - 14,
                                 height > 72 ? height - 72 : 1);
            user::gui::text(x + 20, y + 42, "PCKOS isletim sistemi",
                            user::gui::COLOR_TITLE);

            char cpu[80] = "CPU: ";
            uint32_t at = 5;
            append_text(cpu, sizeof(cpu), &at,
                info.cpu_brand[0] != '\0' ? info.cpu_brand : info.cpu_vendor);
            user::gui::text(x + 20, y + 72, cpu, user::gui::COLOR_TEXT);
            user::gui::text(x + 20, y + 96,
                            "Mimari: i386, 32-bit | Tek cekirdek",
                            user::gui::COLOR_TEXT);

            char memory[96] = "Bellek: ";
            at = 8;
            append_number(memory, sizeof(memory), &at, info.total_pages / 256);
            append_text(memory, sizeof(memory), &at, " MiB toplam, ");
            append_number(memory, sizeof(memory), &at, info.free_pages / 256);
            append_text(memory, sizeof(memory), &at, " MiB bos");
            user::gui::text(x + 20, y + 120, memory, user::gui::COLOR_TEXT);

            char heap[80] = "Cekirdek heap kullanimi: ";
            at = 25;
            append_number(heap, sizeof(heap), &at, info.kernel_heap_used);
            append_text(heap, sizeof(heap), &at, " bayt");
            user::gui::text(x + 20, y + 144, heap, user::gui::COLOR_TEXT);

            char active[80] = "Aktif surec: ";
            at = 13;
            append_number(active, sizeof(active), &at, user::process_count());
            append_text(active, sizeof(active), &at, " | thread: ");
            append_number(active, sizeof(active), &at, user::thread_count());
            user::gui::text(x + 20, y + 168, active, user::gui::COLOR_TEXT);

            user::gui::text(x + 20, y + 192,
                "Programlar: her uygulama ayri process ve ana thread olarak calisir",
                user::gui::COLOR_TEXT);
            user::gui::text(x + 20, y + 216,
                "Thread sistemi: tek cekirdekte Round Robin, PIT 100 Hz",
                user::gui::COLOR_TEXT);
            user::gui::text(x + 20, y + 240,
                "Dosya sistemi: PCKFS v1, IDE ATA PIO, salt okunur",
                user::gui::COLOR_TEXT);

            char display[80] = "Grafik: ";
            at = 8;
            append_number(display, sizeof(display), &at, graphics.width);
            append_text(display, sizeof(display), &at, " x ");
            append_number(display, sizeof(display), &at, graphics.height);
            append_text(display, sizeof(display), &at,
                        " framebuffer, coklu pencere");
            user::gui::text(x + 20, y + 264, display, user::gui::COLOR_TEXT);
            user::gui::text(x + 20, y + 288,
                "Klavye/fare: PS/2 | Cizim: kullanici alani grafik kutuphanesi",
                user::gui::COLOR_TEXT);

            const bool filesystem_ready = user::root_inode() >= 0;
            user::gui::text(x + 20, y + 304,
                filesystem_ready
                ? "Durum: PCKFS kok dizini bagli ve erisilebilir"
                    : "Durum: PCKFS kok dizini kullanilamiyor",
                filesystem_ready ? user::gui::COLOR_TITLE : user::gui::COLOR_TEXT);
            user::gui::pop_clip();
            user::gui::button(x + width - 92, y + height - 34, 76, 24, "Tamam");
        }

        void render_console()
        {
            user::gui::begin_frame();
            user::graphics_clear(user::gui::COLOR_BLACK);
            user::gui::text(12, 8, "PCKOS KONSOLU - ESC: masaustune don",
                            0xFFC0FFC0);
            terminal_.draw(8, 30, screen_.width - 16,
                           screen_.height > 40 ? screen_.height - 40 : 1);
            user::gui::end_frame();
        }

        void render(const WindowBounds *damage = nullptr)
        {
            if (console_mode_)
            {
                render_console();
                return;
            }
            user::gui::begin_frame();
            if (damage != nullptr)
                user::gui::push_clip(static_cast<uint32_t>(damage->x),
                    static_cast<uint32_t>(damage->y), damage->width, damage->height);
            user::graphics_clear(user::gui::COLOR_DESKTOP);
            draw_icon_label(18, 22, 0, "PCKOS");
            draw_icon_label(18, 96, 1, "Dosyalar");
            draw_icon_label(18, 170, 2, "Terminal");
            draw_icon_label(18, 244, 3, "Gorevler");
            if (logo_available_ && screen_.width > 150)
                user::gui::bitmap(screen_.width - 148, 68,
                                  logo_pixels_, 128, 128);
            if (file_visible_ && active_ != WINDOW_FILE)
                draw_window(WINDOW_FILE);
            if (terminal_visible_ && active_ != WINDOW_TERMINAL)
                draw_window(WINDOW_TERMINAL);
            if (task_visible_ && active_ != WINDOW_TASK_MANAGER)
                draw_window(WINDOW_TASK_MANAGER);
            if (file_visible_ && active_ == WINDOW_FILE)
                draw_window(WINDOW_FILE);
            if (terminal_visible_ && active_ == WINDOW_TERMINAL)
                draw_window(WINDOW_TERMINAL);
            if (task_visible_ && active_ == WINDOW_TASK_MANAGER)
                draw_window(WINDOW_TASK_MANAGER);
            draw_taskbar();
            if (menu_open_)
                draw_start_menu();
            if (about_open_)
                draw_about();
            if (damage != nullptr) user::gui::pop_clip();
            user::gui::end_frame();
        }

        void redraw_window(uint32_t id)
        {
            // Hasarlı dikdörtgenin içeriğini katman sırasına göre yeniden birleştir;
            // buna uygulama içeriğinin üzerine gelebilen menü ve pencereler de dahildir.
            render(&bounds(id));
        }

        void handle_key(int32_t key)
        {
            if (key == 0x1B)
            {
                if (console_mode_)
                    console_mode_ = false;
                else if (menu_open_)
                    menu_open_ = false;
                else if (about_open_)
                    about_open_ = false;
                else if (active_ == WINDOW_FILE && file_visible_)
                {
                    file_visible_ = false;
                    file_taskbar_ = false;
                    active_ = terminal_visible_ ? WINDOW_TERMINAL : 0;
                }
                else if (active_ == WINDOW_TERMINAL && terminal_visible_)
                {
                    terminal_visible_ = false;
                    terminal_taskbar_ = false;
                    active_ = fallback_window();
                }
                else if (active_ == WINDOW_TASK_MANAGER && task_visible_)
                {
                    task_visible_ = false;
                    task_taskbar_ = false;
                    active_ = fallback_window();
                }
                else
                    console_mode_ = true;
                dragging_ = false;
                render();
                return;
            }

            if (menu_open_ || about_open_) return;
            if (console_mode_)
            {
                terminal_.post_key(key);
            }
            else if (!about_open_ && active_ == WINDOW_FILE && file_visible_)
            {
                file_manager_.post_key(key);
            }
            else if (!about_open_ && active_ == WINDOW_TERMINAL && terminal_visible_)
            {
                terminal_.post_key(key);
            }
        }

        void activate(uint32_t id)
        {
            active_ = id;
            menu_open_ = false;
            about_open_ = false;
        }

        void dispatch_window_click(uint32_t id, int32_t x, int32_t y)
        {
            WindowBounds &window = bounds(id);
            const int32_t right = window.x + static_cast<int32_t>(window.width);
            if (y >= window.y + 5 && y < window.y + 25 &&
                x >= right - 27 && x < right - 6)
            {
                if (id == WINDOW_FILE)
                {
                    file_visible_ = false;
                    file_taskbar_ = false;
                }
                else if (id == WINDOW_TERMINAL)
                {
                    terminal_visible_ = false;
                    terminal_taskbar_ = false;
                }
                else
                {
                    task_visible_ = false;
                    task_taskbar_ = false;
                }
                active_ = fallback_window();
                render();
                return;
            }
            if (y >= window.y + 5 && y < window.y + 25 &&
                x >= right - 47 && x < right - 28)
            {
                if (id == WINDOW_FILE) file_visible_ = false;
                else if (id == WINDOW_TERMINAL) terminal_visible_ = false;
                else task_visible_ = false;
                active_ = fallback_window();
                render();
                return;
            }
            if (y >= window.y + 4 && y < window.y + 28 && x < right - 48)
            {
                dragging_ = true;
                drag_window_ = id;
                drag_offset_x_ = x - window.x;
                drag_offset_y_ = y - window.y;
                return;
            }
            const int32_t client_x = window.x + 7;
            const int32_t client_y = window.y + 32;
            const uint32_t client_width = window.width > 14
                ? window.width - 14 : 1;
            const uint32_t client_height = window.height > 40
                ? window.height - 40 : 1;
            if (id == WINDOW_FILE)
                file_manager_.post_click(x - client_x, y - client_y,
                                         client_width, client_height);
        }

        void handle_click()
        {
            const uint32_t taskbar_y = screen_.height > TASKBAR_HEIGHT
                ? screen_.height - TASKBAR_HEIGHT : 0;
            const int32_t x = mouse_x_;
            const int32_t y = mouse_y_;
            if (about_open_)
            {
                about_open_ = false;
                render();
                return;
            }
            if (menu_open_)
            {
                const uint32_t menu_y = screen_.height > TASKBAR_HEIGHT + 282
                    ? screen_.height - TASKBAR_HEIGHT - 282 : 0;
                if (x >= 51 && x < 225 && y >= static_cast<int32_t>(menu_y + 8) &&
                    y < static_cast<int32_t>(menu_y + 42))
                    open_window(WINDOW_FILE);
                else if (x >= 51 && x < 225 &&
                         y >= static_cast<int32_t>(menu_y + 46) &&
                         y < static_cast<int32_t>(menu_y + 80))
                    open_window(WINDOW_TERMINAL);
                else if (x >= 51 && x < 225 &&
                         y >= static_cast<int32_t>(menu_y + 84) &&
                         y < static_cast<int32_t>(menu_y + 118))
                    open_window(WINDOW_TASK_MANAGER);
                else if (x >= 51 && x < 225 &&
                         y >= static_cast<int32_t>(menu_y + 122) &&
                         y < static_cast<int32_t>(menu_y + 156))
                    open_window(WINDOW_CALCULATOR);
                else if (x >= 51 && x < 225 &&
                         y >= static_cast<int32_t>(menu_y + 160) &&
                         y < static_cast<int32_t>(menu_y + 194))
                    open_window(WINDOW_NOTEPAD);
                else if (x >= 51 && x < 225 &&
                         y >= static_cast<int32_t>(menu_y + 198) &&
                         y < static_cast<int32_t>(menu_y + 232))
                {
                    menu_open_ = false;
                    about_open_ = true;
                    render();
                }
                else if (x >= 51 && x < 225 &&
                         y >= static_cast<int32_t>(menu_y + 236) &&
                         y < static_cast<int32_t>(menu_y + 270))
                {
                    menu_open_ = false;
                    console_mode_ = true;
                    render();
                }
                else
                {
                    menu_open_ = false;
                    render();
                }
                return;
            }

            if (y >= static_cast<int32_t>(taskbar_y))
            {
                if (x >= 5 && x < 103)
                {
                    menu_open_ = true;
                    render();
                }
                else if (file_taskbar_ && x >= 110 && x < static_cast<int32_t>(110 + task_button_width()))
                {
                    (void)restore_app(WINDOW_FILE);
                }
                else if (terminal_taskbar_ && x >= static_cast<int32_t>(terminal_task_x()) &&
                         x < static_cast<int32_t>(terminal_task_x() + task_button_width()))
                {
                    (void)restore_app(WINDOW_TERMINAL);
                }
                else if (task_taskbar_ &&
                         x >= static_cast<int32_t>(task_manager_task_x()) &&
                         x < static_cast<int32_t>(task_manager_task_x() + task_button_width()))
                {
                    (void)restore_app(WINDOW_TASK_MANAGER);
                }
                else if (calculator_taskbar_ &&
                         x >= static_cast<int32_t>(calculator_task_x()) &&
                         x < static_cast<int32_t>(calculator_task_x() + task_button_width()))
                {
                    (void)restore_app(WINDOW_CALCULATOR);
                }
                else if (notepad_taskbar_ &&
                         x >= static_cast<int32_t>(notepad_task_x()) &&
                         x < static_cast<int32_t>(notepad_task_x() + task_button_width()))
                {
                    (void)restore_app(WINDOW_NOTEPAD);
                }
                return;
            }

            const uint32_t order[4] = {
                active_, WINDOW_FILE, WINDOW_TERMINAL, WINDOW_TASK_MANAGER
            };
            for (uint32_t i = 0; i < 4; ++i)
            {
                const uint32_t id = order[i];
                if (id == 0 || !visible(id)) continue;
                WindowBounds &window = bounds(id);
                if (x >= window.x && y >= window.y &&
                    x < window.x + static_cast<int32_t>(window.width) &&
                    y < window.y + static_cast<int32_t>(window.height))
                {
                    if (id != active_)
                    {
                        activate(id);
                        render();
                    }
                    dispatch_window_click(id, x, y);
                    return;
                }
            }

            if (x >= 18 && x < 72 && y >= 22 && y < 86)
            {
                about_open_ = true;
                render();
            }
            else if (x >= 18 && x < 72 && y >= 96 && y < 160)
                open_window(WINDOW_FILE);
            else if (x >= 18 && x < 72 && y >= 170 && y < 234)
                open_window(WINDOW_TERMINAL);
            else if (x >= 18 && x < 72 && y >= 244 && y < 308)
                open_window(WINDOW_TASK_MANAGER);
        }

        void handle_mouse(const user::InputEvent &event)
        {
            int32_t cursor_x = mouse_x_;
            int32_t cursor_y = mouse_y_;
            if (user::graphics_get_cursor(&cursor_x, &cursor_y) == 0)
            {
                mouse_x_ = cursor_x;
                mouse_y_ = cursor_y;
            }
            mouse_x_ += event.value0;
            mouse_y_ -= event.value1;
            if (mouse_x_ < 0) mouse_x_ = 0;
            if (mouse_y_ < 0) mouse_y_ = 0;
            if (mouse_x_ >= static_cast<int32_t>(screen_.width))
                mouse_x_ = static_cast<int32_t>(screen_.width) - 1;
            if (mouse_y_ >= static_cast<int32_t>(screen_.height))
                mouse_y_ = static_cast<int32_t>(screen_.height) - 1;
            user::graphics_cursor(static_cast<uint32_t>(mouse_x_),
                                  static_cast<uint32_t>(mouse_y_), true);

            if (console_mode_)
            {
                dragging_ = false;
                last_buttons_ = static_cast<uint8_t>(event.buttons);
                return;
            }
            const bool pressed = (event.buttons & 1U) != 0 &&
                ((last_buttons_ & 1U) == 0 || (event.buttons & 0x100U) != 0);
            if (pressed)
                handle_click();

            if ((event.buttons & 1U) != 0 && dragging_)
            {
                WindowBounds &window = bounds(drag_window_);
                const WindowBounds old = window;
                window.x = mouse_x_ - drag_offset_x_;
                window.y = mouse_y_ - drag_offset_y_;
                clamp_window(&window);
                if (old.x != window.x || old.y != window.y)
                {
                    const int32_t left = old.x < window.x ? old.x : window.x;
                    const int32_t top = old.y < window.y ? old.y : window.y;
                    const int32_t right = (old.x > window.x ? old.x : window.x) +
                                          static_cast<int32_t>(window.width);
                    const int32_t bottom = (old.y > window.y ? old.y : window.y) +
                                           static_cast<int32_t>(window.height);
                    const WindowBounds damage{left, top,
                        static_cast<uint32_t>(right - left),
                        static_cast<uint32_t>(bottom - top)};
                    render(&damage);
                }
            }
            if ((event.buttons & 1U) == 0)
                dragging_ = false;
            last_buttons_ = static_cast<uint8_t>(event.buttons);
        }

        user::GraphicsInfo screen_;
        user::apps::FileManager file_manager_;
        user::apps::Terminal terminal_;
        user::apps::TaskManager task_manager_;
        uint32_t app_process_ids_[6];
        uint32_t foreground_window_;
        bool logo_available_;
        uint32_t logo_pixels_[128 * 128];
        bool file_visible_;
        bool terminal_visible_;
        bool file_taskbar_;
        bool terminal_taskbar_;
        bool task_visible_;
        bool task_taskbar_;
        bool calculator_taskbar_;
        bool notepad_taskbar_;
        bool about_open_;
        bool menu_open_;
        bool console_mode_;
        bool dragging_;
        uint32_t drag_window_;
        uint32_t active_;
        int32_t mouse_x_;
        int32_t mouse_y_;
        uint8_t last_buttons_;
        int32_t drag_offset_x_;
        int32_t drag_offset_y_;
        WindowBounds file_bounds_;
        WindowBounds terminal_bounds_;
        WindowBounds task_bounds_;
    };
}

extern "C" [[noreturn]] __attribute__((section(".text._start"))) void _start()
{
    user::GraphicsInfo screen{};
    if (user::graphics_get_info(&screen) < 0 ||
        screen.width < 400 || screen.height < 240)
        user::exit(-1);
    DesktopSession session;
    session.run();
}
