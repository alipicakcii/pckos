// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) PCK
#include <user/graphics.hh>
#include <user/input.hh>
#include <user/process.hh>
#include <user/system.hh>
#include <user/thread.hh>
#include <user/gui.hh>
#include <user/lib/window.hh>
#include <user/audio.hh>
#include <user/lib/new.hh>

namespace
{
    constexpr uint32_t BOARD_WIDTH = 20;
    constexpr uint32_t BOARD_HEIGHT = 13;
    constexpr uint32_t BOARD_CAPACITY = BOARD_WIDTH * BOARD_HEIGHT;
    constexpr uint32_t FOOD_COUNT = 4;
    static_assert(sizeof(uint32_t) == 4, "Bellek hucresi 4 bayt olmali");
    constexpr uint32_t DIRECTION_RIGHT = 0;
    constexpr uint32_t DIRECTION_DOWN = 1;
    constexpr uint32_t DIRECTION_LEFT = 2;
    constexpr uint32_t DIRECTION_UP = 3;

    struct Point
    {
        int32_t x;
        int32_t y;
    };

    void play_food_sound(void *);

    struct AllocationBlock
    {
        uint32_t *values[64];
        uint32_t used;
        AllocationBlock *next;
    };

    class MemoryHunterGame
    {
    public:
        MemoryHunterGame()
            : length_(6), direction_(DIRECTION_RIGHT),
              foods_{{14, 6}, {3, 2}, {17, 4}, {4, 10}},
              random_state_(0), started_(false), paused_(false), game_over_(false),
              allocations_(nullptr), allocation_count_(0), last_allocation_(nullptr),
              memory_error_(false)
        {
            body_[0] = {10, 6};
            body_[1] = {9, 6};
            body_[2] = {8, 6};
            body_[3] = {7, 6};
            body_[4] = {6, 6};
            body_[5] = {5, 6};
        }

        ~MemoryHunterGame() { release_memory(); }

        void release_memory()
        {
            while (allocations_ != nullptr)
            {
                AllocationBlock *block = allocations_;
                allocations_ = block->next;
                for (uint32_t i = 0; i < block->used; ++i)
                    delete block->values[i];
                delete block;
            }
            allocation_count_ = 0;
            last_allocation_ = nullptr;
        }

        uint32_t allocated_bytes() const { return allocation_count_ * sizeof(uint32_t); }
        const uint32_t *last_allocation() const { return last_allocation_; }
        bool memory_error() const { return memory_error_; }

        uint32_t score() const { return length_ > 6 ? length_ - 6 : 0; }
        bool started() const { return started_; }
        bool paused() const { return paused_; }
        bool game_over() const { return game_over_; }
        uint32_t direction() const { return direction_; }
        uint32_t length() const { return length_; }
        const Point &body(uint32_t index) const { return body_[index]; }
        const Point &food(uint32_t index) const { return foods_[index]; }

        void key(int32_t value)
        {
            uint32_t next = direction_;
            if (value == 0xE5 || value == 'd' || value == 'D')
                next = DIRECTION_RIGHT;
            else if (value == 0xE3 || value == 's' || value == 'S')
                next = DIRECTION_DOWN;
            else if (value == 0xE4 || value == 'a' || value == 'A')
                next = DIRECTION_LEFT;
            else if (value == 0xE2 || value == 'w' || value == 'W')
                next = DIRECTION_UP;
            else if (value == '\n' || value == ' ')
            {
                if (!started_ || game_over_)
                    reset();
                else if (paused_)
                    paused_ = false;
                return;
            }
            else if (value == 'p' || value == 'P')
            {
                if (started_ && !game_over_)
                    paused_ = !paused_;
                return;
            }
            else
                return;

            if (!started_ || game_over_)
            {
                reset(next);
                return;
            }
            if ((next + 2) % 4 != direction_)
                direction_ = next;
        }

        void step()
        {
            if (!started_ || paused_ || game_over_)
                return;
            Point next = body_[0];
            if (direction_ == DIRECTION_RIGHT)
                ++next.x;
            else if (direction_ == DIRECTION_DOWN)
                ++next.y;
            else if (direction_ == DIRECTION_LEFT)
                --next.x;
            else
                --next.y;

            if (next.x < 0 || next.x >= static_cast<int32_t>(BOARD_WIDTH) ||
                next.y < 0 || next.y >= static_cast<int32_t>(BOARD_HEIGHT))
            {
                game_over_ = true;
                return;
            }

            uint32_t eaten_food = FOOD_COUNT;
            for (uint32_t i = 0; i < FOOD_COUNT; ++i)
                if (next.x == foods_[i].x && next.y == foods_[i].y)
                {
                    eaten_food = i;
                    break;
                }
            const bool eating = eaten_food < FOOD_COUNT;

            const uint32_t collision_length = eating ? length_ : length_ - 1;
            for (uint32_t i = 0; i < collision_length; ++i)
                if (body_[i].x == next.x && body_[i].y == next.y)
                {
                    game_over_ = true;
                    return;
                }

            if (eating && !allocate_cell())
            {
                memory_error_ = true;
                game_over_ = true;
                return;
            }
            const uint32_t new_length = eating ? length_ + 1 : length_;
            for (uint32_t i = new_length - 1; i > 0; --i)
                body_[i] = body_[i - 1];
            body_[0] = next;
            length_ = new_length;

            if (eating)
            {
                (void)user::thread_create(&play_food_sound, nullptr, "Yem sesi");
                place_food(eaten_food);
            }
        }

    private:
        bool allocate_cell()
        {
            if (allocations_ == nullptr || allocations_->used == 64)
            {
                AllocationBlock *block = new (std::nothrow) AllocationBlock{};
                if (block == nullptr)
                    return false;
                block->next = allocations_;
                allocations_ = block;
            }
            uint32_t *cell = new (std::nothrow) uint32_t(score() + 1);
            if (cell == nullptr)
                return false;
            allocations_->values[allocations_->used++] = cell;
            ++allocation_count_;
            last_allocation_ = cell;
            return true;
        }

        void reset(uint32_t initial_direction = DIRECTION_RIGHT)
        {
            length_ = 6;
            direction_ = initial_direction;
            if (direction_ == DIRECTION_RIGHT)
            {
                body_[0] = {10, 6};
                body_[1] = {9, 6};
                body_[2] = {8, 6};
                body_[3] = {7, 6};
                body_[4] = {6, 6};
                body_[5] = {5, 6};
            }
            else if (direction_ == DIRECTION_LEFT)
            {
                body_[0] = {9, 6};
                body_[1] = {10, 6};
                body_[2] = {11, 6};
                body_[3] = {12, 6};
                body_[4] = {13, 6};
                body_[5] = {14, 6};
            }
            else if (direction_ == DIRECTION_DOWN)
            {
                body_[0] = {10, 5};
                body_[1] = {10, 4};
                body_[2] = {10, 3};
                body_[3] = {10, 2};
                body_[4] = {10, 1};
                body_[5] = {10, 0};
            }
            else
            {
                body_[0] = {10, 7};
                body_[1] = {10, 8};
                body_[2] = {10, 9};
                body_[3] = {10, 10};
                body_[4] = {10, 11};
                body_[5] = {10, 12};
            }
            random_state_ = static_cast<uint32_t>(user::get_ticks()) ^ 0x50434B4FU;
            started_ = true;
            paused_ = false;
            game_over_ = false;
            memory_error_ = false;
            for (uint32_t i = 0; i < FOOD_COUNT; ++i)
                foods_[i] = {-1, -1};
            for (uint32_t i = 0; i < FOOD_COUNT; ++i)
                place_food(i);
        }

        void place_food(uint32_t food_index)
        {
            random_state_ = random_state_ * 1664525U + 1013904223U;
            const uint32_t start = random_state_ % BOARD_CAPACITY;
            for (uint32_t offset = 0; offset < BOARD_CAPACITY; ++offset)
            {
                const uint32_t cell = (start + offset) % BOARD_CAPACITY;
                const Point candidate{static_cast<int32_t>(cell % BOARD_WIDTH),
                                      static_cast<int32_t>(cell / BOARD_WIDTH)};
                bool occupied = false;
                for (uint32_t i = 0; i < length_; ++i)
                    if (body_[i].x == candidate.x && body_[i].y == candidate.y)
                    {
                        occupied = true;
                        break;
                    }
                for (uint32_t i = 0; !occupied && i < FOOD_COUNT; ++i)
                    if (i != food_index && foods_[i].x == candidate.x &&
                        foods_[i].y == candidate.y)
                        occupied = true;
                if (!occupied)
                {
                    foods_[food_index] = candidate;
                    return;
                }
            }
            game_over_ = true;
        }

        Point body_[BOARD_CAPACITY];
        uint32_t length_;
        uint32_t direction_;
        Point foods_[FOOD_COUNT];
        uint32_t random_state_;
        bool started_;
        bool paused_;
        bool game_over_;
        AllocationBlock *allocations_;
        uint32_t allocation_count_;
        uint32_t *last_allocation_;
        bool memory_error_;
    };

    void play_food_sound(void *)
    {
        user::pc_speaker_play(1000, 30);
    }

    void format_count(char *text, uint32_t capacity, const char *label,
                      uint32_t value)
    {
        uint32_t at = 0;
        while (label[at] != '\0' && at + 1 < capacity)
        {
            text[at] = label[at];
            ++at;
        }
        if (value < 10 && at + 2 < capacity)
            text[at++] = '0';
        char digits[10];
        uint32_t count = 0;
        do
        {
            digits[count++] = static_cast<char>('0' + value % 10);
            value /= 10;
        } while (value != 0 && count < sizeof(digits));
        while (count != 0 && at + 1 < capacity)
            text[at++] = digits[--count];
        text[at] = '\0';
    }

    void draw_count(uint32_t x, uint32_t y, const char *label, uint32_t value)
    {
        char text[40];
        format_count(text, sizeof(text), label, value);
        user::gui::text(x, y, text, 0xFFB8F28B);
    }

    void draw_memory_chip(uint32_t board_x, uint32_t board_y,
                          uint32_t cell_size, const Point &food)
    {
        const uint32_t food_x = board_x + static_cast<uint32_t>(food.x) * cell_size;
        const uint32_t food_y = board_y + static_cast<uint32_t>(food.y) * cell_size;
        const uint32_t chip_size = cell_size > 8 ? cell_size * 3 / 4 : cell_size - 2;
        const uint32_t chip_x = food_x + (cell_size - chip_size) / 2;
        const uint32_t chip_y = food_y + (cell_size - chip_size) / 2;
        user::gui::fill(chip_x, chip_y, chip_size, chip_size, 0xFFB8F265);
        if (chip_size > 4)
        {
            user::gui::fill(chip_x + 2, chip_y + 2,
                            chip_size - 4, chip_size - 4, 0xFF06200D);
            user::gui::fill(chip_x + chip_size / 3, chip_y + chip_size / 3,
                            chip_size / 3, chip_size / 3, 0xFFB8F265);
        }
        const uint32_t pin_width = chip_size / 4 > 1 ? chip_size / 4 : 2;
        for (uint32_t pin = 0; pin < 3; ++pin)
        {
            const uint32_t pin_x = chip_x + 2 + pin * (chip_size > 6 ? (chip_size - 6) / 2 : 1);
            user::gui::fill(pin_x, chip_y - 2, pin_width, 2, 0xFFB8F265);
            user::gui::fill(pin_x, chip_y + chip_size, pin_width, 2, 0xFFB8F265);
        }
        user::gui::fill(chip_x - 2, chip_y + chip_size / 3, 2, pin_width,
                        0xFFB8F265);
        user::gui::fill(chip_x - 2, chip_y + chip_size * 2 / 3, 2, pin_width,
                        0xFFB8F265);
        user::gui::fill(chip_x + chip_size, chip_y + chip_size / 3, 2, pin_width,
                        0xFFB8F265);
        user::gui::fill(chip_x + chip_size, chip_y + chip_size * 2 / 3, 2, pin_width,
                        0xFFB8F265);
    }

    void draw_game(user::gui::Window &window, uint32_t x, uint32_t y,
                   uint32_t width, uint32_t height,
                   uint32_t cell_size, const MemoryHunterGame &game)
    {
        window.draw();
        const uint32_t content_x = x + 7;
        const uint32_t content_y = y + 32;
        const uint32_t board_width = BOARD_WIDTH * cell_size;
        const uint32_t board_height = BOARD_HEIGHT * cell_size;
        const uint32_t board_x = x + (width - board_width) / 2;
        const uint32_t board_y = content_y + 58;
        user::gui::push_clip(content_x, content_y, width - 14, height - 40);
        user::gui::fill(content_x, content_y, width - 14, height - 40,
                        0xFF001608);

        draw_count(content_x + 14, content_y + 12, "Puan: ", game.score());
        const char *state =
            game.memory_error() ? "Bellek dolu" : game.game_over() ? "Oyun bitti" : game.paused() ? "Duraklatildi"
                                          : game.started()  ? "Oynaniyor"
                                                            : "Hazir";
        user::gui::text(content_x + 150, content_y + 12, state,
                        game.game_over() ? 0xFFFF6464 : 0xFFB8F28B);
        char memory_count[40];
        format_count(memory_count, sizeof(memory_count),
                     "Veri (bayt): ", game.allocated_bytes());
        const uint32_t memory_x = content_x + width - 21 -
                                  user::gui::text_width(memory_count);
        user::gui::text(memory_x, content_y + 12, memory_count,
                        0xFFB8F28B);
        char address[] = "Son tahsis: 0x00000000";
        const uintptr_t pointer = reinterpret_cast<uintptr_t>(game.last_allocation());
        constexpr char hex[] = "0123456789ABCDEF";
        for (uint32_t i = 0; i < 8; ++i)
            address[14 + i] = hex[(pointer >> ((7 - i) * 4)) & 15];
        user::gui::text(content_x + 14, content_y + 34,
                        game.last_allocation() != nullptr ? address : "Son tahsis: -",
                        0xFFB8F28B);
        user::gui::button(x + width - 130, content_y + 31,
                          116, 22, "Oyun bilgisi");

        user::gui::fill(board_x, board_y, board_width, board_height, 0xFF001608);
        user::gui::fill(board_x, board_y, board_width, 2, 0xFF39D85E);
        user::gui::fill(board_x, board_y + board_height - 2,
                        board_width, 2, 0xFF39D85E);
        user::gui::fill(board_x, board_y, 2, board_height, 0xFF39D85E);
        user::gui::fill(board_x + board_width - 2, board_y,
                        2, board_height, 0xFF39D85E);
        for (uint32_t column = 1; column < BOARD_WIDTH; ++column)
            user::gui::fill(board_x + column * cell_size, board_y + 2,
                            1, board_height - 4, 0xFF124020);
        for (uint32_t row = 1; row < BOARD_HEIGHT; ++row)
            user::gui::fill(board_x + 2, board_y + row * cell_size,
                            board_width - 4, 1, 0xFF124020);

        for (uint32_t i = 0; i < FOOD_COUNT; ++i)
            draw_memory_chip(board_x, board_y, cell_size, game.food(i));
        for (uint32_t i = 0; i < game.length(); ++i)
        {
            const Point &segment = game.body(i);
            const uint32_t segment_x = board_x + static_cast<uint32_t>(segment.x) * cell_size;
            const uint32_t segment_y = board_y + static_cast<uint32_t>(segment.y) * cell_size;
            const uint32_t color = i == 0 ? 0xFF9AFF61 : 0xFF39C85C;
            user::gui::fill(segment_x + 2, segment_y + 2,
                            cell_size - 4, cell_size - 4, color);
            if (i == 0)
            {
                if (game.direction() == DIRECTION_RIGHT || game.direction() == DIRECTION_LEFT)
                {
                    const uint32_t eye_x = segment_x +
                                           (game.direction() == DIRECTION_RIGHT ? cell_size - 8 : 5);
                    user::gui::fill(eye_x, segment_y + 7, 3, 3, 0xFF012A11);
                    user::gui::fill(eye_x, segment_y + cell_size - 10,
                                    3, 3, 0xFF012A11);
                }
                else
                {
                    const uint32_t eye_y = segment_y +
                                           (game.direction() == DIRECTION_DOWN ? cell_size - 8 : 5);
                    user::gui::fill(segment_x + 7, eye_y, 3, 3, 0xFF012A11);
                    user::gui::fill(segment_x + cell_size - 10, eye_y,
                                    3, 3, 0xFF012A11);
                }
            }
        }

        const char *hint = game.game_over()
                               ? "Enter: yeni tur | I: bilgi | Esc: cikis"
                               : "Yon tuslari | P: duraklat | I: bilgi | Esc: cikis";
        user::gui::text(content_x + 14, y + height - 25, hint,
                        0xFFB8F28B);
        user::gui::pop_clip();
    }

    void info_bounds(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                     uint32_t &left, uint32_t &top, uint32_t &panel_width,
                     uint32_t &panel_height)
    {
        panel_width = width - 24 < 520 ? width - 24 : 520;
        panel_height = height - 56 < 342 ? height - 56 : 342;
        left = x + (width - panel_width) / 2;
        top = y + 30 + (height - 30 - panel_height) / 2;
    }

    void draw_game_info(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
    {
        uint32_t left, top, panel_width, panel_height;
        info_bounds(x, y, width, height, left, top, panel_width, panel_height);
        user::gui::frame(left, top, panel_width, panel_height, user::gui::COLOR_FACE);
        user::gui::fill(left + 3, top + 3, panel_width - 6, 24, user::gui::COLOR_TITLE);
        user::gui::text(left + 12, top + 7, "Bellek Avcisi - Oyun bilgisi", user::gui::COLOR_WHITE);
        const char *lines[] = {
            "Her yem: new uint32_t ile 4 bayt ayirir.",
            "Hucrenin degeri, o yemdeki puaninizdir.",
            "Son tahsis, kullanici heap'indeki sanal adrestir.",
            "Veri (bayt), toplanan hucrelerin toplamidir.",
            "Takip listesi ve ayirici ek bellek kullanir.",
            "Yeni turda onceki tahsisler korunur.",
            "ESC veya X ile cikista delete ile temizlenir.",
            "Yon tuslari: hareket | P: duraklat",
            "Bu aciklama acikken oyun bekler."
        };
        user::gui::push_clip(left + 10, top + 34, panel_width - 20, panel_height - 76);
        uint32_t line_y = top + 38;
        const uint32_t columns = (panel_width - 28) / user::gui::FONT_WIDTH;
        for (const char *line : lines)
        {
            uint32_t offset = 0;
            while (line[offset] != '\0')
            {
                char row[64];
                uint32_t count = 0;
                while (line[offset + count] != '\0' && count < columns && count < 63)
                {
                    row[count] = line[offset + count];
                    ++count;
                }
                row[count] = '\0';
                user::gui::text(left + 14, line_y, row, user::gui::COLOR_TEXT);
                offset += count;
                line_y += 22;
            }
        }
        user::gui::pop_clip();
        user::gui::button(left + (panel_width - 96) / 2,
                          top + panel_height - 34, 96, 24, "Tamam");
    }

    void release_game_memory(void *context)
    {
        static_cast<MemoryHunterGame *>(context)->release_memory();
    }
}

extern "C" [[noreturn]] __attribute__((section(".text._start"))) void _start()
{
    user::GraphicsInfo screen{};
    if (user::graphics_get_info(&screen) < 0 ||
        screen.width < 400 || screen.height < 300)
        user::exit(-1);
    user::thread_set_name("Bellek Avcisi");

    const uint32_t usable_height = screen.height - 48;
    uint32_t width = screen.width * 58 / 100;
    if (width < 600) width = 600;
    if (width > 840) width = 840;
    if (width > screen.width - 20) width = screen.width - 20;
    uint32_t height = usable_height * 69 / 100;
    if (height < 500) height = 500;
    if (height > 680) height = 680;
    if (height > usable_height - 10) height = usable_height - 10;
    uint32_t x = (screen.width - width) / 2 + 20;
    uint32_t y = (usable_height - height) / 2;
    if (x + width > screen.width)
        x = screen.width - width;
    if (y + height > usable_height)
        y = usable_height - height;
    uint32_t cell_size = (width - 40) / BOARD_WIDTH;
    const uint32_t height_cell = (height - 120) / BOARD_HEIGHT;
    if (height_cell < cell_size)
        cell_size = height_cell;
    if (cell_size > 25)
        cell_size = 25;
    if (cell_size < 6)
        cell_size = 6;

    user::gui::Window window(x, y, width, height, "PCKOS - Bellek Avcisi");
    if (!window.valid())
        user::exit(-1);
    MemoryHunterGame game;
    window.set_close_callback(release_game_memory, &game);
    bool info_open = false;
    int32_t mouse_x = static_cast<int32_t>(screen.width / 2);
    int32_t mouse_y = static_cast<int32_t>(screen.height / 2);
    (void)user::graphics_get_cursor(&mouse_x, &mouse_y);
    user::graphics_cursor(mouse_x, mouse_y, true);
    uint32_t last_buttons = 0;
    uint32_t last_step = static_cast<uint32_t>(user::get_ticks());

    for (;;)
    {
        if (user::process_is_minimized())
        {
            last_buttons = 0;
            user::InputEvent ignored{};
            (void)user::wait_event(&ignored, 10);
            last_step = static_cast<uint32_t>(user::get_ticks());
            continue;
        }

        user::gui::begin_frame();
        x = window.x();
        y = window.y();
        draw_game(window, x, y, width, height, cell_size, game);
        if (info_open)
            draw_game_info(x, y, width, height);
        user::gui::end_frame();

        user::InputEvent event{};
        (void)user::wait_event(&event, 1);
        if (event.type == 1)
        {
            if (event.value0 == 'i' || event.value0 == 'I')
            {
                info_open = !info_open;
                last_step = static_cast<uint32_t>(user::get_ticks());
                continue;
            }
            if (event.value0 == 0x1B)
            {
                if (info_open)
                {
                    info_open = false;
                    last_step = static_cast<uint32_t>(user::get_ticks());
                    continue;
                }
                game.release_memory();
                user::exit(0);
            }
            if (info_open && event.value0 == '\n')
                info_open = false;
            else if (!info_open)
                game.key(event.value0);
        }
        else if (event.type == 2)
        {
            int32_t current_x = mouse_x, current_y = mouse_y;
            if (user::graphics_get_cursor(&current_x, &current_y) == 0)
            {
                mouse_x = current_x;
                mouse_y = current_y;
            }
            mouse_x += event.value0;
            mouse_y -= event.value1;
            if (mouse_x < 0)
                mouse_x = 0;
            if (mouse_y < 0)
                mouse_y = 0;
            if (mouse_x >= static_cast<int32_t>(screen.width))
                mouse_x = static_cast<int32_t>(screen.width) - 1;
            if (mouse_y >= static_cast<int32_t>(screen.height))
                mouse_y = static_cast<int32_t>(screen.height) - 1;
            user::graphics_cursor(mouse_x, mouse_y, true);
            const bool pressed = (event.buttons & 1U) != 0 &&
                                 ((last_buttons & 1U) == 0 || (event.buttons & 0x100U) != 0);
            if (pressed && !info_open &&
                mouse_x >= static_cast<int32_t>(x + width - 130) &&
                mouse_x < static_cast<int32_t>(x + width - 14) &&
                mouse_y >= static_cast<int32_t>(y + 63) &&
                mouse_y < static_cast<int32_t>(y + 85))
                info_open = true;
            else if (pressed && info_open)
            {
                uint32_t left, top, panel_width, panel_height;
                info_bounds(x, y, width, height, left, top, panel_width, panel_height);
                const uint32_t button_x = left + (panel_width - 96) / 2;
                const uint32_t button_y = top + panel_height - 34;
                if (mouse_x >= static_cast<int32_t>(button_x) &&
                    mouse_x < static_cast<int32_t>(button_x + 96) &&
                    mouse_y >= static_cast<int32_t>(button_y) &&
                    mouse_y < static_cast<int32_t>(button_y + 24))
                    info_open = false;
            }
            if (window.handle_mouse(mouse_x, mouse_y, event.buttons, pressed,
                                    screen.width, usable_height))
            { last_buttons = event.buttons; continue; }
            last_buttons = event.buttons;
        }

        const uint32_t now = static_cast<uint32_t>(user::get_ticks());
        if (info_open)
        {
            last_step = now;
            continue;
        }
        const uint32_t speed = game.score() > 6 ? 4 : 10 - game.score();
        if (game.started() && !game.paused() && !game.game_over() &&
            now - last_step >= speed)
        {
            game.step();
            last_step = now;
        }
    }
}
