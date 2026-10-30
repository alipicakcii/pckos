# PCKOS

[Türkçe](README.md) | [English](README.en.md)

**PCKOS**, bir sistem programcısı olarak hobi amacıyla sıfırdan geliştirmeye başladığım bir işletim sistemi çekirdeği projesidir.
Yaklaşık yedi yıl önce, üretken yapay zekâ araçlarının henüz hayatımızda olmadığı bir dönemde, kendi işletim sistemi çekirdeğimi yazmaya başladım.
Projeye hiçbir zaman tam zamanlı olarak odaklanma fırsatım olmadı. Son iki yıldır da geliştirme sürecine büyük ölçüde ara vermiş durumdayım.
Artık PCKOS'u açık kaynak olarak paylaşmaya karar verdim. Amacım, projeye ilgi duyan insanlarla bir topluluk oluşturmak ve yeterli ilgi olursa geliştirmeye kaldığım yerden devam etmek. :)

Daha fazla bilgi için: [PCKOS blog yazım](https://pckblog.com/pckos.html)



PCKOS şu an minimal ve geliştirme aşamasında olan bir sistemdir. Mevcut çekirdeğin başlıca yetenekleri:

- i386 üzerinde açılma, bellek sayfalama ve çekirdek/kullanıcı modu ayrımı.
- Sistem çağrıları üzerinden çekirdek hizmetlerine erişen kullanıcı uygulamaları.
- Tek çekirdekte çalışan, Round Robin zamanlamalı thread'ler ve ayrı adres alanları olan process'ler.
- Framebuffer tabanlı masaüstü; klavye, fare, pencereler ve görev çubuğu.
- PCKFS kök dosya sistemi ve IDE ATA PIO disk sürücüsü. Kullanıcı uygulamaları şu an dosyaları okuyabilir; genel dosya yazma desteği henüz yoktur.
- Masaüstünden açılan Terminal, Dosyalar, Görev Yöneticisi, Hesap Makinesi ve salt okunur Not Defteri.
- RTC'den tarih/saat okuma; Görev Yöneticisi'nde bellek, işlemci ve thread bilgileri.

## Katkı ve hata bildirimi

Yapay zekâ kullanımına karşı değilim; projeye yapılacak katkılarda yapay zekâ araçlarından yararlanılabilir. Tek beklentim, katkıyı gönderen kişinin yaptığı değişiklikleri anlaması ve açıklayabilmesidir.


## Lisans

Başka bir lisans bildirimi bulunmayan PCKOS kodu GNU Genel Kamu Lisansı'nın 3. sürümü veya (tercihinize bağlı olarak) daha sonraki sürümleri altında lisanslanır. Tam metin için [`LICENSE`](LICENSE) dosyasına bak. Üçüncü taraf varlıkların kendi lisans bildirimleri geçerlidir.

## Ekran görüntüleri

### Masaüstü, Dosyalar ve Görev Yöneticisi

![PCKOS masaüstünde Dosyalar ve Görev Yöneticisi pencereleri](assets/screenshots/qemu-desktop.png)

### Terminal

![PCKOS masaüstünde açık Terminal penceresi](assets/screenshots/qemu-terminal.png)

### Birden fazla uygulama

![PCKOS'ta Not Defteri, Hesap Makinesi ve Terminal pencereleri](assets/screenshots/qemu-apps.png)

## QEMU'da çalıştırma

QEMU kurulu Linux ana makinede, proje kaynak kodunu aldıktan ve derleme gereksinimlerini kurduktan sonra proje dizininde çalıştır:

```sh
./qemu.sh
```

Betik çekirdeği ve uygulamaları derler, `pckos.iso` ile `pckfs.img` dosyalarını üretir ve QEMU'yu başlatır. KVM kullanılabiliyorsa etkinleştirir; değilse TCG emülasyonuna döner. Gerekli derleme araçları aşağıdaki teknik dokümantasyonun “Derleme ve çalışma” bölümünde listelenmiştir.

Depodaki `host-share/` örnek dosyalarını PCKFS imajına ekler. Kendi dosyalarını da aynı klasöre koyarak imajda kullanabilirsin.

Önceden derlenmiş `pckos.iso` ve `pckfs.img` dosyalarını indirdiğinde, bu iki dosyayı aynı dizine koyup şu komutla başlatabilirsin:

```sh
qemu-system-i386 -m 2G \
  -cdrom pckos.iso \
  -drive file=pckfs.img,format=raw,if=ide,index=0,media=disk
```

ISO çekirdeği başlatır; `pckfs.img` PCKOS kök dosya sistemidir. İkisi de gereklidir.

## VirtualBox'ta çalıştırma

`pckos.iso` ve `pckfs.img` dosyalarını indirin. `pckfs.img` dosyasını VirtualBox diskine dönüştürün:

```sh
VBoxManage convertfromraw pckfs.img pckfs-vbox.vdi --format VDI
```

Kaynak koddan imajları hazırlıyorsan proje dizininde:

```sh
./iso.sh
./share.sh
./vbox.sh
```

Bu komutlar ISO'yu ve PCKFS disk imajını üretir; `vbox.sh`, `pckfs.img` dosyasını VirtualBox'ın kullanacağı `pckfs-vbox.vdi` biçimine dönüştürür.

VirtualBox'ta yeni bir **Linux / Other Linux (32-bit)** sanal makinesi oluştur. Belleği 2 GiB ayarla, grafik denetleyicisini **VBoxVGA** seç ve 3D hızlandırmayı kapalı tut. Boş bir sabit disk oluşturma. Depolama ayarlarında `pckos.iso` dosyasını optik sürücüye, `pckfs-vbox.vdi` dosyasını IDE **Primary Master** olarak ekle. Açılış sırasını optik sürücüden başlatacak şekilde ayarla. ISO sistemi başlatır; VDI kök dosya sistemini sağlar.

## Geliştirme ortamı

PCKOS'u Kali Linux üzerinde geliştiriyorum. C++ kodunu, GCC 13.1.0 kaynak kodundan derlediğim GNU `i686-elf-g++` çapraz derleyicisiyle; assembly kodunu NASM 3.01 ile derliyorum. Kali Linux kullanılan geliştirme ana makinesidir; PCKOS'un hedef mimarisi i386'dır.

# Teknik dokümantasyon

Bu belge PCKOS'un **mevcut çalışan kodunu** anlatır. Yeni bir özellik eklerken önce “Nerede ne var?” tablosuna bak; sonraki bölümlerde açılış, PCKFS, sistem çağrıları, process/thread ve yeni uygulama örneği bulunur. Komutlar proje kök dizininde çalıştırılır.

## 1. Açılış: ilk komuttan masaüstüne

```text
QEMU/BIOS → GRUB → Multiboot _start → kernel_main
          → GDT/IDT + bellek + sürücüler + PCKFS + PIT
          → kullanıcı adres alanı → Ring 3 masaüstü
          → PCKFS'ten ayrı uygulama süreçleri
```

1. `./qemu.sh`, `pckos.iso` dosyasını CD-ROM, `pckfs.img` dosyasını IDE disk olarak QEMU'ya bağlar. `iso.sh` tarafından yazılan `isodir/boot/grub/grub.cfg`, GRUB'a `pckos.bin` çekirdeğini ve 1024×768×32 grafik modunu söyler.
2. `kernel/arch/i386/boot.asm` içindeki Multiboot başlığı GRUB tarafından okunur. `_start` ilk 16 KiB çekirdek yığınını kurar, Multiboot bilgi adresini `kernel_main()` fonksiyonuna verir. `kernel/arch/i386/linker.ld` çekirdeği 1 MiB adresine bağlar.
3. `kernel/kernel.cpp` kesmeleri kapatır; seri portu ve işlemci tablolarını kurar, sayfalamayı başlatır. `_init()`, çekirdek heap'i hazır olduktan sonra global C++ kurucularını çalıştırır.
4. Klavye, fare, framebuffer, ATA PIO disk ve PCKFS başlatılır. `_kprintf` ile açılış bilgileri çekirdek günlüğüne/seri porta yazılır. PIT 100 Hz ayarlanır.
5. `kernel/user_mode.cpp` kullanıcı sayfalarını, kullanıcı yığınını ve sistem çağrılarını hazırlar; `kernel/thread.cpp` zamanlayıcıyı kurar. `user_mode_run_desktop()` masaüstü imajını `0x40000000` adresine yükler. `kernel/arch/i386/user_mode.asm` içindeki `iret` Ring 3'e geçer.
6. Masaüstü `kernel/user/apps/desktop.cpp` içindeki `_start` ile çalışır. Dosyalar, Terminal, Görevler, Hesap Makinesi veya Not Defteri açıldığında `process_spawn()` çağrılır; çekirdek `/apps/*.bin` imajını PCKFS'ten yükler, yeni PID, adres alanı ve ana thread oluşturur.

**İki yükleme yolu var:** Masaüstü `kernel/arch/i386/user_desktop.asm` içindeki `incbin` ile çekirdeğe gömülüdür. Diğer uygulamalar çekirdeğe gömülmez; PCKFS diskinden yüklenir. Masaüstü ve uygulamalar Ring 3'te, sürücüler ve syscall işleyicisi Ring 0'da çalışır.

## 2. Nerede ne var?

| Konum | Burada ne yapılır? |
| --- | --- |
| `kernel/arch/i386/` | Multiboot girişi, GDT/IDT, kesme ve Ring 3 geçişi, bağlantı betikleri |
| `kernel/kernel.cpp` | Çekirdek başlatma sırası |
| `kernel/mem/` | Sayfalama, fiziksel bellek, çekirdek heap'i |
| `kernel/interrupts/`, `kernel/thread.cpp` | IRQ/PIT, Round Robin zamanlama, PID/TID tabloları |
| `kernel/device/ata_pio.cpp` | IDE diskten sektör okuma/yazma |
| `kernel/device/rtc.cpp` | CMOS RTC'den gerçek tarih ve saati okuma |
| `kernel/fs/pckfs.cpp` | PCKFS bağlama, yol çözme, dizin ve dosya okuma |
| `kernel/user_mode.cpp` | Kullanıcı adres alanları, program yükleme, process ve kullanıcı thread'i oluşturma |
| `kernel/syscall.cpp` | `int 0x80` çağrı dağıtımı ve kullanıcı belleği doğrulaması |
| `kernel/include/user/syscall.hh` | Kullanıcı programlarının syscall sarmalayıcıları ve veri yapıları |
| `kernel/include/user/gui.hh`, `kernel/user/gui/graphics.cpp` | Ring 3 ortak çizim API'si |
| `kernel/device/video/frame_buffer.cpp` | Ekran belleği ve pencerelerin katmanlarla birleştirilmesi |
| `kernel/user/apps/desktop.cpp` | Masaüstü, simgeler, Başlat, görev çubuğu ve uygulama başlatma |
| `kernel/user/apps/*_process.cpp` | Her uygulamanın `_start` ve olay döngüsü |
| `kernel/user/apps/terminal.cpp`, `file_manager.cpp`, `task_manager.cpp` | Uygulama içeriği/iş mantığı |
| `tools/mkpckfs.py`, `host-share/` | Disk imajı üretimi ve host'tan aktarılacak dosyalar |

Çekirdek heap'i büyürken yeni sayfalar önce ana çekirdek sayfa dizinine (`KERN_PAGE_DIRECTORY`) eşlenir. `Vmm::sync_all_from_kernel()` bu eşlemeleri açık kullanıcı süreçlerine yayar. Sayfa hatasında `serial` günlüğündeki `[PageFault]` satırı hatalı adresi (`CR2`), komut adresini (`EIP`), hata kodunu ve etkin sayfa dizinini (`CR3`) gösterir.

## 3. Derleme ve çalışma

Gereken araçlar: `i686-elf-g++`, `i686-elf-objcopy`, `nasm`, `make`, `grub-mkrescue`, `python3`, `qemu-system-i386`. Ortak ayarlar `config.sh` ve `kernel/Makefile` içindedir.

| Komut | Etkisi |
| --- | --- |
| `./build.sh` | Çekirdeği/uygulamaları derler, çekirdeği `sysroot/boot/` altına kurar. |
| `./iso.sh` | `build.sh` çalıştırır, `pckos.iso` üretir. |
| `./share.sh` | `build.sh` çalıştırır; `host-share/`, sistem uygulamaları ve logodan `pckfs.img` üretir. |
| `./qemu.sh` | Temizler, ISO ve diski yeniden üretir, QEMU'yu açar. |
| `./vbox.sh` | Mevcut `pckfs.img` dosyasını `pckfs-vbox.vdi` biçimine çevirir. Önce güncel imajı `./qemu.sh` veya `./share.sh` ile üret. |
| `./clean.sh` | Derleme ve imaj çıktılarını temizler; `host-share/` korunur. |

`sysroot/` ve `isodir/` sadece hazırlık alanlarıdır. Çalışan sistemin kökü `pckfs.img` içindeki `/` dizinidir. `initrd` kullanılmaz. `.elf` bağlantı çıktısıdır; çalışan yükleyicinin aldığı imaj `objcopy` ile üretilmiş **düz `.bin`** dosyasıdır. `qemu.sh` erişim varsa KVM, yoksa TCG kullanır. Seri çıktı proje kökündeki `serial` dosyasındadır.

`vbox.sh` mevcut `pckfs-vbox.vdi` çıktısını kaldırıp `convertfromraw` ile tekrar üretir. Dönüşümde sanal makineye kayıtlı sabit disk UUID'sini kullanır; böylece VirtualBox'ın disk kaydı bozulmaz. ISO'yu, çekirdeği veya VirtualBox makine ayarlarını değiştirmez. Yeniden üretim sırasında VirtualBox makinesi kapalı olsun.

## 4. PCKFS v1: basit teknik açıklama

`tools/mkpckfs.py` varsayılan olarak 32 MiB ham disk imajı üretir. `share.sh` önce `host-share/` ağacını `sysroot/pckfs-source/` içine kopyalar; derlenmiş uygulamaları `/apps/` altına ekler. `assets/logo.bmp` otomatik olarak `/system/logo.bmp` olur. Varsayılan dizinler `/boot`, `/dev`, `/etc`, `/home`, `/mnt`, `/tmp`, `/var`; `/apps` sistem uygulamalarından oluşur. Bunlar şu an adlandırılmış dizinlerdir; `/dev` için aygıt düğümü semantiği gibi Unix özellikleri yoktur.

Diskin fiziksel düzeni:

```text
LBA 0: süperblok (512 bayt, magic "PCKFS1\0\0", sürüm 1)
LBA 1...: inode tablosu (256 kayıt × 64 bayt)
sonra: blok zinciri tablosu (her veri bloğu için 32 bit sonraki blok numarası)
sonra: veri bölgesi (blok başına 4096 bayt = 8 sektör)
```

Süperblok toplam sektör, inode/FAT/veri başlangıçlarını ve kök inode numarasını taşır. Her inode 48 bayt ad, dosya boyutu, ilk veri bloğu, üst dizin inode'u, tür ve kullanım bayrağı içerir. Tür `1` dosya, `2` dizindir; kök inode `0`'dır. FAT benzeri zincirde `0xFFFFFFFF` son blok işaretidir. Veri bloğu numarası **1'den** başlar; blok 1'in ilk sektörü `data_start_sector` konumudur. Dizinler, kendi içerikleri için ayrı blok taşımaz: çocuklar inode tablosunda `parent` alanı karşılaştırılarak bulunur. Dosya/dizin adı en çok 47 UTF-8 bayttır. Sembolik bağlantı yoktur.

Açılışta `AtaPioDisk::initialize()` IDE diski bulur. `pckfs_mount()` süperblok, boyutlar, inode'lar ve FAT'i doğrular. `pckfs_find()` `/a/b` yolunu bileşenlere böler; `pckfs_get_child()` dizini listeler; `pckfs_read_file_range()` blok zincirini izleyip sektörleri okur. Üretici `tools/mkpckfs.py`, okuyucu `kernel/fs/pckfs.cpp` ve kullanıcıya sunulan çağrılar `kernel/syscall.cpp` içindedir. **Disk biçimini değiştirirken bu üçünü birlikte güncelle.** Sürüm veya inode yerleşimi değişirse eski imajın bağlanmasını engellemek için doğrulamayı da değiştir.

PCKFS v1 şu anda kullanıcı programları için **salt okunur**. ATA sürücüsünde sektör yazma işlevi bulunsa da PCKFS'te genel dosya oluşturma/yazma/silme ve bunlar için syscall henüz yoktur. Host'tan dosya aktarmak için:

```sh
mkdir -p host-share/home/pck
cp ornek.txt host-share/home/pck/ornek.txt
./qemu.sh
```

Dosya PCKOS'ta `/home/pck/ornek.txt` olur. Dosyalar uygulamasında görünür; Terminal'de `cat /home/pck/ornek.txt` ile okunabilir. `qemu.sh` diski her seferinde yeniden ürettiği için kaynak dosyayı `host-share/` içinde tut.

Kullanıcı programında okuma örneği:

```cpp
#include <user/syscall.hh>

char buffer[128];
const int32_t fd = user::open("/home/pck/ornek.txt"); // flags=0, okuma
if (fd >= 0) {
    const int32_t count = user::read(fd, buffer, sizeof(buffer));
    if (count > 0)
        user::write(user::stdout_descriptor, buffer,
                    static_cast<uint32_t>(count));
    user::close(fd);
}
```

`stdout` ve `stderr` masaüstü penceresine değil çekirdek günlüğüne/seri porta gider. Dizin için `root_inode()` ve `read_directory(inode, index, &entry)` kullan: dönüş `1` öğe, `0` liste sonu, negatif değer hatadır. `DirectoryEntry::type == user::directory_type` dizini gösterir.

## 5. SDK ve sistem çağrıları

Kullanıcı tarafında kullanılabilen API'nin kaynağı `kernel/include/user/syscall.hh`; grafik yardımcısı `kernel/include/user/gui.hh` ve `kernel/user/gui/graphics.cpp` dosyalarıdır. Henüz bağımsız paketlenmiş SDK veya standart C/C++ çalışma kütüphanesi yoktur.

`user::call()`, `int 0x80` çalıştırır. `EAX` çağrı numarası, `EBX/ECX/EDX/ESI/EDI` en çok beş argüman, sonuç yine `EAX` içindedir. `kernel/syscall.cpp` çağrıyı dağıtır; kullanıcı işaretçilerini ve uzunlukları doğrular. Hatalar negatif sayı olarak döner. Kodda sayıları elle kullanmak yerine sarmalayıcı fonksiyonları çağır.

| Alan | Önemli API'ler |
| --- | --- |
| Dosya | `open`, `read`, `close`, `root_inode`, `read_directory` |
| Günlük/giriş | `write`, `get_kernel_log`, `wait_event` |
| Process | `process_spawn`, `process_count`, `process_get_info`, `process_focus`, `process_minimize`, `exit` |
| Thread | `thread_create`, `thread_id`, `thread_sleep`, `thread_yield`, `thread_exit`, `thread_get_info` |
| Sistem | `get_ticks`, `get_datetime`, `get_heap_usage`, `get_system_info` |
| Grafik | `graphics_get_info`, `graphics_register_window`, `graphics_begin_frame`, `graphics_end_frame`, `graphics_rect`, `graphics_text`, `graphics_bitmap`, `graphics_push_clip`, `graphics_pop_clip` |

`wait_event(&event, ticks)`: `type=1` klavye (`value0` karakter), `type=2` fare (`value0` X farkı, `value1` Y farkı), `type=3` zaman aşımıdır. `buttons & 1` sol tuş; `buttons & 0x100` yeni basma kenarıdır. 100 PIT tik'i yaklaşık bir saniyedir. Çizim ekran koordinatlarıyla yapılır. Pencereli bir uygulama önce `graphics_register_window(x,y,w,h)` çağırır; sonra bir kareyi `user::gui::begin_frame()` ve `end_frame()` arasında çizer. `user::gui::window`, `fill`, `text`, `button`, `icon` ve `push_clip` hazır yardımcılarıdır.

`get_datetime(&date)` RTC'deki yıl, ay, gün, saat, dakika ve saniyeyi `user::DateTime` yapısına doldurur. Görev çubuğundaki saat açılıştan beri geçen süreyi değil sanal makinenin RTC saatini gösterir; QEMU betiği RTC'yi host yerel saatiyle başlatır. Hesap Makinesi ve Not Defteri Başlat menüsünden açılır; ikisi de ayrı PID ve ana thread alır. Hesap Makinesi tam sayı işlemleri (`+`, `-`, `*`, `/`) yapar. Not Defteri, yol alanına yazılan herhangi bir PCKFS metin dosyasının ilk 8191 baytını **salt okunur** gösterir. Örneğin `host-share/home/notlar.txt` konursa yol `/home/notlar.txt` olur; dosya adının kodda özel bir anlamı yoktur.

```cpp
user::InputEvent event{};
if (user::wait_event(&event, 10) > 0 &&
    event.type == 1 && event.value0 == 0x1B)
    user::exit(0); // ESC
```

Yeni syscall eklerken: numarayı ve kullanıcı sarmalayıcısını `kernel/include/user/syscall.hh` içine; işleyiciyi `kernel/syscall.cpp` içindeki dağıtıcıya ekle. Kullanıcıdan gelen adresleri `is_user_range` ile denetle. Gerekiyorsa asıl çekirdek işlevini `kernel/` altında ilgili modülde uygula. Dönüş kodu ve sınırları burada belgele.

## 6. Process nasıl oluşturulur?

**Process**, ayrı PID ve sayfa tablosu/adres alanı olan programdır. Açılışta çekirdek masaüstü process'ini başlatır; diğerleri yalnızca açılınca oluşturulur. Kullanıcı programından:

```cpp
const int32_t pid = user::process_spawn("/apps/terminal.bin", "Terminal");
if (pid < 0) {
    const char message[] = "Baslatma hatasi\n";
    user::write(user::stderr_descriptor, message, sizeof(message) - 1);
}
```

`process_spawn` sonucu başarılıysa yeni PID'dir. Çağrı uygulamanın bitmesini beklemez. Çekirdekte akış: `kernel/syscall.cpp` kullanıcı yolunu/adını kopyalar → `user_mode_spawn_process()` PCKFS'te dosyayı bulup düz ikiliyi okur → boş adres alanı ayırır ve kod/yığın sayfalarını eşler → `process_create()` PID verir → `thread_create_user()` ana TID'yi oluşturur → giriş odağı yeni process'e geçer. Bunlar sırasıyla `kernel/user_mode.cpp` ve `kernel/thread.cpp` içindedir. Programın `_start` fonksiyonu başlar; `user::exit(status)` veya ana thread'in bitmesi process'i sonlandırır. Kapanınca kaynakları ve pencere katmanı serbest bırakılır.

Süreçleri listeleme:

```cpp
for (uint32_t i = 0; i < user::process_count(); ++i) {
    user::ProcessInfo info{};
    if (user::process_get_info(i, &info) == 0) {
        // info.id, parent_id, main_thread_id, thread_count, cpu_ticks, name
    }
}
```

Bir process'i öne getirmek için `process_focus(pid)`, kendi penceresini küçültmek için `process_minimize()` vardır. Masaüstü uygulama türü başına tek PID izlediğinden aynı simge tekrar basılınca mevcut pencere öne gelir.

## 7. Thread nasıl oluşturulur?

**Thread**, aynı process'in PID'sini ve adres alanını paylaşır; ayrı TID, kullanıcı yığını ve çekirdek kesme yığını alır. Tek çekirdekli PCKOS'ta PIT 100 Hz ve Round Robin zamanlayıcı thread'ler arasında geçiş yapar. Aynı anda fiziksel olarak tek thread yürür; birden fazla thread sırayla CPU kullanır.

```cpp
#include <user/syscall.hh>

void worker(void *argument)
{
    volatile uint32_t *counter =
        static_cast<volatile uint32_t *>(argument);
    for (;;) {
        ++*counter;
        user::thread_sleep(100); // yaklaşık 1 saniye
    }
}

volatile uint32_t counter = 0;
const int32_t tid = user::thread_create(worker, (void *)&counter, "Sayac");
if (tid < 0) {
    // Thread oluşturulamadı.
}
```

`thread_create` başarılıysa TID döndürür. Kernel uygun bir thread yeri ve yeni kullanıcı yığını ayırır, giriş fonksiyonu ile argümanı yığına koyar; `thread_start_trampoline` fonksiyon bitince `thread_exit` çağırır. Çalışan örnek: `kernel/user/apps/task_manager_process.cpp` içindeki `TaskManager::thread_entry`; ana thread pencereyi çizerken worker thread bilgileri toplar. Paylaşılan veriler için mevcut `kernel/include/user/apps/app_thread.hh` içindeki `SpinLock`/`LockGuard` örneğini incele. Genel amaçlı `join`, mutex ve semaphore API'si henüz yoktur. Ana thread/process bitince diğer thread'ler de temizlenir.

## 8. Yeni uygulama yazma, derleme ve başlatma

Örnek dosya: `kernel/user/apps/merhaba_process.cpp`. PCKOS'un yüklediği düz ikilinin giriş noktası `_start` olmalıdır:

```cpp
#include <user/gui.hh>
#include <user/syscall.hh>

extern "C" [[noreturn]]
__attribute__((section(".text._start"))) void _start()
{
    constexpr uint32_t x = 260, y = 180, width = 360, height = 180;
    if (user::graphics_register_window(x, y, width, height) < 0)
        user::exit(1);
    user::thread_set_name("Merhaba");

    for (;;) {
        user::gui::begin_frame();
        user::gui::window(x, y, width, height, "PCKOS - Merhaba", true);
        user::gui::text(x + 22, y + 60, "Merhaba PCKOS!",
                        user::gui::COLOR_TEXT);
        user::gui::text(x + 22, y + 92, "ESC ile kapat",
                        user::gui::COLOR_TEXT);
        user::gui::end_frame();

        user::InputEvent event{};
        if (user::wait_event(&event, 10) > 0 &&
            event.type == 1 && event.value0 == 0x1B)
            user::exit(0);
    }
}
```

Mevcut uygulama bağlantı düzeniyle derle ve PCKFS kaynağına koy:

```sh
./build.sh  # Grafik kitaplığını proje ayarlarıyla çapraz derler.
i686-elf-g++ -m32 -ffreestanding -Wall -Wextra -fno-exceptions -fno-rtti \
  -fno-use-cxa-atexit -fno-pic -fno-pie -fno-stack-protector -fno-builtin \
  -fno-unwind-tables -fno-asynchronous-unwind-tables -mno-sse -mno-mmx \
  -msoft-float -Ikernel/include \
  -c kernel/user/apps/merhaba_process.cpp -o kernel/user/apps/merhaba_process.user.o
i686-elf-g++ -m32 -no-pie -nostdlib \
  -Wl,-T,kernel/user/apps/linker.ld -Wl,--build-id=none \
  -o kernel/user/apps/merhaba_process.elf \
  kernel/user/gui/graphics.user.o kernel/user/apps/merhaba_process.user.o
mkdir -p host-share/apps
i686-elf-objcopy -O binary kernel/user/apps/merhaba_process.elf \
  host-share/apps/merhaba.bin
./share.sh
```

Burada `.elf` sadece bağlantı ara çıktısı, `host-share/apps/merhaba.bin` PCKFS'te `/apps/merhaba.bin` olur. **Uygulamayı çekirdeğe linklemek gerekmez.** Masaüstünden başlatmak için bir kullanıcı tarafı başlatma noktası gerekir. Hızlı deneme yolu: `kernel/user/apps/desktop.cpp` sonundaki `_start()` içinde `DesktopSession session;` satırından sonra geçici olarak şu satırı ekle:

```cpp
(void)user::process_spawn("/apps/merhaba.bin", "Merhaba");
```

Sonra `./qemu.sh` çalıştır. Masaüstü başlarken uygulama da açılır. ESC odak bu penceredeyken onu kapatır. Kalıcı bir masaüstü simgesi veya Başlat girdisi için `desktop.cpp` içindeki `open_window()`, `handle_click()`, görev çubuğu çizimi ve `app_process_ids_` takibini yeni uygulamaya genişlet. Mevcut üç uygulama bunun somut örneğidir. `qemu.sh` her defasında `clean.sh` çalıştırdığı için yeni uygulamanın derleme adımlarını kalıcı hale getirmek istersen `kernel/Makefile` içine mevcut `*_process.elf/.bin` kuralları gibi hedef ekle; ardından `share.sh` içine `/apps` kopyalama satırı ekle.

## 9. Bugünkü teknik sınırlar ve kontrol

- Yükleyici ELF ayrıştırmaz: düz `.bin`, `_start` ve `0x40000000` bağlantı adresi kullanılır. İmaj en çok 64 sayfa (256 KiB) olabilir.
- En çok 8 process kaydı ve 16 scheduler thread girdisi vardır; masaüstü bir process yerini kullanır. Kullanıcı alanında genel amaçlı heap/`new`, standart kütüphane, process argümanları ve dosya yazma API'si yoktur.
- Çekirdek global C++ kurucularını `_init()` ile başlatır. Derleme `-fno-rtti` ve `-fno-exceptions` kullanır. Yerleşik `dynamic_cast`/`typeid` ve exception'lar mevcut yapılandırmada kullanılamaz. `kernel/include/library/pck_rtti.hh` içindeki `pckos::dynamic_cast_to<T>()` yalnızca uygun `pckos_is_a()` hiyerarşileri içindir.
- GUI'de pencere kaydı ve çizim katmanları vardır; uygulama türü başına tek açık kopya izlenir.

```sh
sh tools/tests/run_framebuffer_tests.sh
python3 tools/test_app_layout.py
# Önce ISO ve PCKFS imajlarını üret:
python3 tools/tests/qemu_gui_smoke.py
```

QEMU ekran kontrolünün görüntü/günlük çıktıları `tools/tests/gui-output/` içine yazılır. Normal kullanım için `./qemu.sh` çalıştırıp masaüstünü ve `serial` günlüğünü incele.

## Sıradaki işler

Bu liste planlanan işleri gösterir; tamamlanmış özellikleri değil.

### 1. Temeli sağlamlaştır

- Süreç başına dosya tanıtıcıları, okuma konumları ve kapanış temizliği ekle.
- Uygulama kapanması/çökmesi, pencere odağı, küçültme ve sürükleme davranışlarını tutarlı hale getir.
- Çoklu pencere, klavye, süreç/thread sınırları, sayfa hataları ve kaynak temizliği için tekrarlanabilir QEMU kontrollerini genişlet.

### 2. Dosya yazma ve kalıcılık

- Kullanıcı verisini her açılışta yeniden üretilen imajdan ayır; `clean.sh` kullanıcı verisini silmemeli.
- PCKFS'e genel dosya oluşturma, yazma, kısaltma ve silme; boş blok yönetimi ve dolu disk/yarım yazma durumlarını ekle.
- Dosya sistemi çağrılarına `write`, `seek`, `truncate`, `mkdir` ve `unlink` ekle; Dosyalar'da oluşturma/yeniden adlandırma ve Not Defteri'nde genel kaydetmeyi destekle.

### 3. Uygulama geliştirmeyi kolaylaştır

- Düz `.bin` yüklemesinden statik i386 ELF32 yükleyicisine geç; `PT_LOAD`, BSS, sayfa izinleri ve giriş adresini doğrula. [ELF yükleme belirtimini](https://gabi.xinuos.com/elf/07-loading-intro.html) temel al.
- Bellek ayırma, metin/sayı dönüşümü, süreç argümanları ve ortak pencere/olay yardımcıları içeren küçük bir kullanıcı alanı SDK'sı hazırla.
- Sabit 8 süreç/16 thread sınırını ve uygulama başına tek pencere varsayımını kaldır.

### 4. Masaüstünü olgunlaştır

- Türkçe karakter/Unicode, klavye düzeni, metin seçimi, imleç hareketi ve fare tekerleği desteğini geliştir.
- Pencere boyutlandırma, odak sırası, kısayollar ve uygulama olay kuyruklarını tamamla; hızlı etkileşimlerde titreşim ve olay kaybını önle.
