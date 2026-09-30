# Gözle Programlama IDE (GPI)

**Düğümleri birbirine bağla, gerçek C# veya C++ koduna dönüştür, tek tuşla derle ve çalıştır.**

GPI, Windows üzerinde çalışan, C++20 ile yazılmış bir **görsel programlama ortamıdır (visual programming IDE)**.
Programın gerçek kaynağı bir grafiktir (düğümler + kablolar); **metin kodu bu graftan türetilir.**
Üretilen kod gerçek bir C# veya C++ projesi olarak diske yazılır, gerçek derleyiciyle derlenir ve
çalıştırılır. Yani "ekran görüntüsü alıp kod yazan" bir araç değil, **gerçekten derlenen** bir araçtır.

| | |
|---|---|
| Sürüm | 1.0.0 (`gp_version()`) |
| Dil | C++20 (çekirdek + arayüz), C# / C++ (üretilen kod) |
| Derleme sistemi | CMake ≥ 3.22, MSVC (Visual Studio 2022) |
| Arayüz | Dear ImGui + Direct3D 11 (Win32) |
| Düğüm kataloğu | **88** yerleşik düğüm tipi |
| Birim testi | **66** test, hepsi geçiyor |
| Platform | Windows 10/11 · x64 |

> **Yön tek taraflıdır:** bugün **görsel → kod** dönüşümü vardır. Kod paneli bir *önizlemedir* ve
> salt okunurdur; koda yazmak görseli değiştirmez. Kod → görsel yönü henüz uygulanmamıştır.
> Ayrıntılar için [Bilinen sınırlamalar](#9-bilinen-sınırlamalar).

---

## İçindekiler

1. [Ne yapar?](#1-ne-yapar)
2. [Hızlı başlangıç (5 adım)](#2-hızlı-başlangıç-5-adım)
3. [Gereksinimler](#3gereksinimler)
4. [Derleme](#4-derleme)
5. [Çalıştırma ve komut satırı seçenekleri](#5-çalıştırma-ve-komut-satırı-seçenekleri)
6. [Doğrulama araçları: `gpcli` ve `gp_tests`](#6-doğrulama-araçları-gpcli-ve-gp_tests)
7. [Ekranı kullanma](#7-ekranı-kullanma)
8. [Mimari](#8-mimari)
9. [Bilinen sınırlamalar](#9-bilinen-sınırlamalar)
10. [Sorun giderme](#10-sorun-giderme)
11. [Katkı rehberi: yeni düğüm nasıl eklenir?](#11-katkı-rehberi-yeni-düğüm-nasıl-eklenir)
12. [Proje yapısı ve dosya sorumlulukları](#12-proje-yapısı-ve-dosya-sorumlulukları)
13. [Düğüm kataloğu (tam liste)](#13-düğüm-kataloğu-tam-liste)
14. [Üretilen dosyalar ve çalışma dizinleri](#14-üretilen-dosyalar-ve-çalışma-dizinleri)
15. [Tasarım kararları](#15-tasarım-kararları)
16. [Lisans ve teşekkürler](#16-lisans-ve-teşekkürler)

---

## 1. Ne yapar?

GPI, programı bir **düğüm grafi** olarak saklar ve bu graftan gerçek kod üretir:

```
  Metin("Merhaba") ─┐
                   ├─→ Birleştir ─→ Büyük Harfe Çevir ─→ Ekrana Yaz
  Metin(", dünya!") ┘
```

Bu dört düğüm, şu C# koduna dönüşür:

```csharp
string v0 = "Merhaba";
string v1 = ", dünya!";
string v2 = v0 + v1;            // Birleştir
string v3 = v2.ToUpper();       // Büyük Harfe Çevir
Console.WriteLine(v3);
```

ve şu C++ koduna:

```cpp
const std::string v0 = "Merhaba";
const std::string v1 = ", dünya!";
const std::string v2 = (v0 + v1);
const std::string v3 = ([](std::string s) { for (auto& ch : s) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch))); return s; })(v2);
std::cout << v3 << std::endl;
```

**Öne çıkan özellikler**

- **Görsel programlama.** 88 düğüm tipi: matematik, metin, akış, mantık, liste/sözlük, değişken,
  giriş/çıkış, değer ve tanım kategorileri.
- **İki hedef dil.** Aynı graf, tek tıkla C# **veya** C++ olarak üretilir. Düğümler her iki dil için
  ayrı ayrı kod yazar.
- **Gerçek derleme ve çalıştırma.** IDE, `dotnet build` veya `cl.exe` + `vcvars` komutunu kendisi kurar,
  derler, exe'yi çalıştırıp konsol çıktısını panele alır.
- **Anlık hata denetimi.** Graf değiştikçe (tuvali bırakır bırakmaz) arka planda analiz + kod üretimi
  çalışır; hatalı düğüm ve port anında kırmızıya döner, Kod paneli güncellenir. F5'e basman gerekmez.
- **Kod ↔ graf gezinme.** Kod panelinde bir satıra tıklayınca ilgili düğüm tuvalde seçilip parlar.
- **Geri al / yinele.** Her düzenleme işlemi etiketli bir *işlemdir* (transaction); bir kabloyu çekmek
  tek adımda geri alınır.
- **Sürükle-bırak düzenleme.** Kabloyu porttan porta çek, düğümü taşı, seçili düğümü sil.
- **Araç zinciri keşfi.** MSVC, .NET SDK, Roslyn, MSBuild, Windows SDK, NuGet önbelleği ve vcpkg
  otomatik bulunur; eksik olanı panelde açıkça söyler.
- **Klavuzsuz başlangıç.** Program boş bir tuvalle açılır; boş graf ekranında "Örnek dize programını
  aç" düğmesi gerçek düğümlerle dolu çalışan bir program kurar ve çalıştırır.
- **Arayüzsüz doğrulama.** Aynı çekirdek, konsol aracı `gpcli` ve birim testleri olarak derlenir;
  iki hedef dil için uçtan uca derleme + çalıştırma doğrulaması yapılır.

---

## 2. Hızlı başlangıç (5 adım)

**1) Yapılandır ve derle**

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

**2) IDE'yi aç**

```powershell
.\build\Debug\Gpi.Shell.exe
```

> Not: CMake önbelleği `build/` kökünde oluşur, bu yüzden derleme komutunda `build\Debug` **değil**
> `--build build` kullanılır; Visual Studio üreticisi çıktıyı `build\Debug\` altına koyar.

**3) Örnek programı aç**
Boş graf ekranındaki **"Örnek dize programını aç"** düğmesine bas ya da menüden
**Dosya → Örnek Program**'ı seç. Bu, iki metni birleştirip büyük harfe çevirip ekrana yazan gerçek
bir program kurar ve (menüde "Derle ve Çalıştır" açıksa) hemen derleyip çalıştırır.

**4) Kodu incele**
**Kod** panelinde üretilen kaynak, satır numaraları ve sözdizimi renklendirmesiyle görünür.
Satıra tıklarsan tuvaldeki karşılık gelen düğüm seçilir ve kırmızı parlar.

**5) Derle**
Menüden **Derle → Üret ve Derle**. Derleme başarılı olursa exe'nin bulunduğu klasör
Explorer'da açılır (menüden kapatılabilir: **Derle → Derlemede klasörü aç**).
Program çıktısı **Çıktı** panelinde görünür.

---

## 3. Gereksinimler

**Zorunlu**

| Bileşen | Sürüm | Neden |
|---|---|---|
| Windows | 10 / 11, x64 | Arayüz Win32 + D3D11 kullanıyor |
| Visual Studio 2022 | 17.x (Community yeterli) | MSVC `cl.exe` |
| CMake | ≥ 3.22 | Derleme sistemi |
| .NET SDK | 9.x (isteğe bağlı) | Yalnızca **C#** hedefi seçildiğinde gerekir |
| Windows SDK | 10.x | MSVC derleme zinciri |

**Kurulum**

1. Visual Studio 2022 → iş yükü **"Desktop development with C++"** (MSVC + Windows SDK dahil).
2. CMake 3.22+ (Visual Studio ile birlikte gelir veya ayrı kurulur).
3. C# üretmek istiyorsan **.NET SDK 9**.

**İsteğe bağlı**

- **NuGet önbelleği** (`%USERPROFILE%\.nuget\packages`) — paket eklemek istersen.
- **vcpkg** — bulunamazsa IDE yalnızca "kurulu değil" der ve devam eder.

Arayüz dışı her şey (`gpcore`, `gpcli`, `gp_tests`) platformdan bağımsızdır; Linux/macOS'ta
`-DGPI_BUILD_SHELL=OFF` ile yalnızca çekirdek derlenebilir.

---

## 4. Derleme

### 4.1 Visual Studio üreticisi (varsayılan)

```powershell
cmake -S . -B build
cmake --build build --config Debug      # veya --config Release
```

Çıktı: `build\Debug\Gpi.Shell.exe`, `build\Debug\gpcli.exe`, `build\Debug\gp_tests.exe`

### 4.2 Ninja üreticisi

```powershell
cmake -S . -B build-ninja -G Ninja
cmake --build build-ninja
```

### 4.3 Yalnızca çekirdek (arayüz yok)

```powershell
cmake -S . -B build-core -DGPI_BUILD_SHELL=OFF
cmake --build build-core --config Debug
```

Bu modda `Gpi.Shell.exe` üretilmez; `gpcore`, `gpcli`, `gp_tests` derlenir.

### 4.4 Derleyici bayrakları (CMakeLists.txt)

MSVC için: `/utf-8 /W4 /permissive- /EHsc /bigobj /FS` ve tanımlar
`_CRT_SECURE_NO_WARNINGS NOMINMAX UNICODE _UNICODE WIN32_LEAN_AND_MEAN`.
Diğer derleyiciler için: `-Wall -Wextra -Wno-unused-parameter`.

> `/utf-8` kritiktir: kaynak dosyalar Türkçe karakter içerir ve yorumlar ile arayüz metinleri bu
> bayrağı olmadan bozulur.

### 4.5 Hedefler

| Hedef | Tür | Açıklama |
|---|---|---|
| `gpcore` | statik kitaplık | Arayüzden bağımsız çekirdek: graf, analiz, üretim, araç zinciri |
| `Gpi.Shell` | Windows uygulaması | ImGui/DX11 tabanlı IDE (yalnızca `WIN32` ve `GPI_BUILD_SHELL=ON`) |
| `gpcli` | konsol | Arayüzsüz uçtan uca doğrulama |
| `gp_tests` | konsol | Birim testleri (66 test) |

---

## 5. Çalıştırma ve komut satırı seçenekleri

```powershell
.\build\Debug\Gpi.Shell.exe [seçenekler]
```

| Seçenek | Açıklama |
|---|---|
| *(seçenek yok)* | IDE'yi normal şekilde açar |
| `/selftest [raporYolu]` | **Pencereyi açmadan** tüm panel düzenini sınar, raporu dosyaya yazar. Varsayılan: `build/shell-selftest.txt` |
| `/probe [raporYolu] [kare]` | N kare çizim yapar, pencere/font/çizim istatistiklerini ve ImGui hata günlüğünü rapora yazar. Varsayılan: 120 kare |
| `/trace` | Başlangıç akışını `build/startup.log` dosyasına adım adım yazar (takılmada nerede olduğunu bulmak için) |

`/probe` çıktısı, "pencere hiç çizmiyor mu?" sorusunun cevabıdır: kare sayısı, köşe (vertex) sayısı,
her ImGui penceresinin konumu/kirpma durumu, font atlası ve `(hata yok)`/`(boş)` log bölümleri raporda yer alır.

```powershell
# 40 kare çiz ve raporu al
.\build\Debug\Gpi.Shell.exe /probe build\shellprobe.txt 40

# Arayüzsüz düzen sınaması
.\build\Debug\Gpi.Shell.exe /selftest build\selftest.txt
```

---

## 6. Doğrulama araçları: `gpcli` ve `gp_tests`

### 6.1 `gpcli` — arayüzsüz uçtan uca doğrulama

Aynı çekirdeği kullanır: **graf üretir → kodu yazar → gerçekten derler → çalıştırır.**

```powershell
.\build\Debug\gpcli.exe probe              # araç zinciri keşfi
.\build\Debug\gpcli.exe catalog            # 88 düğüm tipini listele
.\build\Debug\gpcli.exe catalog metin      # arama ("metin" → Metin kategorisi)
.\build\Debug\gpcli.exe gen cs   out-cs    # örnek program → C# → derle → çalıştır
.\build\Debug\gpcli.exe gen cpp  out-cpp   # örnek program → C++ → derle → çalıştır
.\build\Debug\gpcli.exe demo cs            # dize demosu (iki metin birleştir, büyüt, yaz)
.\build\Debug\gpcli.exe demo cpp
.\build\Debug\gpcli.exe selftest           # hepsi: probe + catalog + C# + C++
```

`gen` komutunun ürettiği örnek program, döngü ve değişken içerir:

```
toplam değişkenini tanımla
"Merhaba Dunya!" yaz
1'den 10'a: sayıyı yaz, toplamı biriktir
"Toplam: 55" yaz
```

`demo` komutunun ürettiği dize programı (konsol çıktısı):

```
C# : MERHABA, GÖRSEL DÜNYA!
C++: MERHABA, GöRSEL DüNYA!      (std::toupper yalnızca ASCII harfleri büyütür)
```

### 6.2 `gp_tests` — birim testleri

```powershell
.\build\Debug\gp_tests.exe
#  GECTI: 66   BASARISIZ: 0
```

```powershell
# CTest ile
ctest --test-dir build -C Debug --output-on-failure
```

Kapsanan konular: tip çözümleme ve dönüşüm denetimi, graf doğrulama (eksik port, kopya bağlantı,
tip uyuşmazlığı), topolojik çalıştırma sırası, düğüm arama (Türkçe karakter katlama), C# **ve** C++
kod üretimi, proje dosyası (`.csproj` / `.vcxproj`) üretimi, örnek dize programı üretimi.

---

## 7. Ekranı kullanma

### 7.1 Paneller

| Panel | Görevi |
|---|---|
| **Çözüm Gezgini** | Üretilen proje dosyalarını klasör ağacıyla gösterir (proje / kaynak / üretilen / çıktı / denetim) |
| **Katalog** | 88 düğüm tipini kategoriye göre listeler, **Türkçe karaktere duyarlı arama** yapar; tıklayınca düğümü tuvale ekler |
| **Tuval** | Düğüm grafi: düğümler, kablolar, portlar, ızgara, seçim, hata vurguları |
| **Ayrıntılar** | Seçili düğümün portlarını ve özelliklerini düzenler (literal değerler, karşılaştırma operatörü…) |
| **Kod** | Üretilen kaynak; satır numaralı, renklendirilmiş, yatay kaydırmalı, tıklanabilir |
| **Çıktı** | Derleme günlüğü, tanılar (satır/sütun/kod) ve program çıktısı |
| **Araç Zinciri** | Keşfedilen derleyici/derleme sistemi/NuGet/vcpkg durumu |

Panel boyutları sürüklenebilir ve yerleşim `imgui.ini` ile saklanır.

### 7.2 Menüler

- **Dosya** → Yeni Proje · Örnek Program · Aç… · Kaydet · Çıkış
- **Düzen** → Geri Al · Yinele · Tümünü Sil
- **Derle** → Üret ve Derle · Çalıştır · *Derle ve Çalıştır* (kutucuk) · *Derlemede klasörü aç* (kutucuk)
- **Görünüm** → panellerin açılıp kapanması · Yerleşimi Sıfırla

### 7.3 Fare ve tuval

| Hareket | Sonuç |
|---|---|
| Düğümün üstüne tıkla, sürükle | Düğümü taşır |
| Çıkış portundan giriş portuna sürükle | Kabloyu bağlar |
| Bağlı bir giriş portuna tıkla-sürükle | Kabloyu söker, yeniden bağlanabilir |
| Düğümün üstüne gel | Denetim mesajlarını balon olarak gösterir |
| **Boş alana sol tık bas + sürükle** | **Tuvali kaydırır (pan)** |
| Orta tuşla sürükle / `Win` + sol sürükle | Tuvali kaydırır |
| Fare tekerleği | İmlecin altındaki noktayı sabit tutarak yakınlaştır/uzaklaştır (0.35×–2.5×) |
| `Ctrl` + sağ tık | Düğüm ekleme menüsü (konum, imlecin altına göre) |
| Sol tık (düğüm/port dışı) | Seçimi kaldırır |
| "Sığdır" düğmesi | Pan'ı tuvalin sol üst köşesine sabitler |

### 7.4 Boş graf ekranı

Program **doldurulmuş bir demo ile değil, boş bir tuvalle** açılır. Boş graf ekranında üç kısayol vardır:

- **Örnek dize programını aç** → gerçek düğümlerle çalışan bir program kurar (otomatik derler ve çalıştırır)
- **Kataloğa göz at** → Katalog panelini açar
- **İlk metin düğümünü ekle** → görünen merkeze `lit.text` düğümü koyar ve seçer

---

## 8. Mimari

### 8.1 Katmanlar

```
┌───────────────────────────────────────────────────────────────┐
│  shell/  — Arayüz (Dear ImGui + DirectX 11, yalnızca Windows) │
│   main.cpp        pencere, ImGui/DX11 başlatma, olay döngüsü   │
│   App             menüler, dock yerleşimi, panel orkestrasyonu│
│   Platform        Win32 pencere + D3D11 aygıtı/ swapchain     │
│   EditorState     graf, seçim, geri al/yinele, pan/zoom        │
│   LiveAnalyzer    arka plan denetimi + kod önizlemesi (iş parçacığı)
│   BuildService    üret → derle → çalıştır (iş parçacığı)      │
│   panels/*        Katalog, Canvas, Ayrıntılar, Kod, Çıktı, …  │
└───────────────────────────────────────────────────────────────┘
                               │  (saf C++ API, arayüz bağımsız)
┌───────────────────────────────────────────────────────────────┐
│  native/ — Çekirdek (gpcore)                                  │
│   Graph + Types     ara temsili (IR) ve tip sistemi           │
│   Catalog           düğüm kataloğu + arama                     │
│   BuiltinNodes/*    88 yerleşik düğümün tanımı ve emit kodu     │
│   Codegen + Emit    C#/C++ kod üretimi                        │
│   ProjectGen        .csproj / .vcxproj / runtime dosyaları     │
│   Toolchain + Build araç zinciri keşfi, derleme, çalıştırma   │
│   Serialization     graf JSON serileştirme                    │
│   Api.h/.cpp        C ABI (P/Invoke için dışa açılan yüzey)    │
│   Json              bağımlılıksız JSON okuma/yazma             │
└───────────────────────────────────────────────────────────────┘
                               │
┌───────────────────────────────────────────────────────────────┐
│  Üretilen proje: build/out-shell/…  →  dotnet build | cl.exe  │
└───────────────────────────────────────────────────────────────┘
```

### 8.2 Çekirdek veri modeli (IR)

Programın gerçek kaynağı `gpi::Graph`'tır:

```cpp
struct Graph {
  std::vector<NodeInstance> nodes;         // id, typeId, x, y, literals, properties
  std::vector<Edge>         edges;         // PortKey → PortKey
  std::map<std::string, SubGraph> subgraphs;
  std::vector<std::string>  entryNodes;
  std::map<std::string, std::string> variables;
  std::string name;
};

struct NodeInstance {
  std::string id;                            // "n7"
  std::string typeId;                        // "str.concat"
  double x, y;                               // tuval koordinatı
  std::map<std::string,std::string> literals;   // bağlanmamış girişlerin değerleri
  std::map<std::string,std::string> properties; // düğüme özel ayarlar
  std::string comment;
  bool collapsed = false, bypassed = false;
};
```

İki çeşit port vardır:

- **Değer portu** (`PortKind::Value`): veri taşır, tipi vardır (`Int`, `Double`, `String`, `Bool`,
  `Any`, `Object`, `List<T>`, `Dict<K,V>`).
- **Akış (exec) portu** (`PortKind::Exec`): yürütme sırasını belirler, tipi yoktur (`Void`).

**Saf (pure) düğümler** exec portu taşımaz; hesaplanıp ifade olarak gömülür. Yürütme zinciri yalnızca
exec/akış portu olan düğümlerden geçer. Bu ayrım, üretilen kodun okunur kalmasını sağlar: saf
düğümler `v2 = v0 + v1` gibi ifade satırlarına, akış düğümleri ise `if` / `for` / `Console.WriteLine`
gibi deyimlere dönüşür.

### 8.3 Düğüm tanımı ve emit geri çağrıları

Her düğüm iki lambdası tanımlar: biri ifade üretir, diğeri satır yazar.

```cpp
NodeType n = binString("str.concat", "Birleştir", "\uE8BD", "+", "+");
n.emitExpr = [=](EmitContext& c) {
  const std::string a = c.value("A"), b = c.value("B");
  return "(" + a + " " + (c.isCSharp() ? opCs : opCpp) + " " + b + ")";
};
n.emit = [](EmitContext& c) {
  const std::string v = c.local("Result");
  c.bindOutput("Result", v);
  c.line(v + " = " + nt->emitExpr(c) + ";");
  c.markMapping("Result");           // kod paneli eşlemesi
};
```

`EmitContext` üretim sırasında şunları sağlar: giriş değeri (`value`), yerel değişken adı (`local`),
kullanıcı literal'ı (`rawLiteral`), tip adlandırma (`typeName`, `nativeTypeName`), satır yazma (`line`),
gerekli `using`/başlık bildirme, düğüm→satır eşlemesi (`markMapping`) ve özellik okuma (`prop`).

Akış kontrol düğümleri (`flow.if`, `flow.while`, `flow.forrange`, `flow.branch`) `properties["control"]`
ile özel ele alınır: emit edici bu düğümleri gördüğünde blok yapısını kurar, dalların ortak devamını
topolojik sıradan bulur.

### 8.4 Anlık denetim (LiveAnalyzer)

`EditorState::revision_` graf üzerindeki **her** değişiklikte artar. Her karede `App::draw()` şunu yapar:

```
revision değiştiyse  →  graf kopyasını kuyruğa al
arka plan iş parçacığı →  analyze() + generate()   (mikrosaniyeler; arayüzü kilitlemez)
UI iş parçacığı       →  snapshot() ile sonucu kopyala, panelleri besle
```

Sonuç (`Snapshot`): üretilen kod, hata/uyarı listesi, `nodeId → satır no` haritası, hata ve uyarı
sayıları. Paneller aynı kare içinde **aynı** sonucu görür; böylece kare başına birden çok kilit alınmaz.

### 8.5 Üretim, derleme, çalıştırma (BuildService)

`BuildService::start()` grafı **kopyalayarak** bir iş parçacığına devreder (arayüzle yarışmasın diye).
İş parçacığı sırayla:

1. `CodeGenerator::generate(graph, options)` → kaynak metni + tanılar
2. Dosyaları yaz: `build/out-shell/` (C#: `Program.g.cs` + `.csproj`; C++: `native/main.cpp` + `native/GpiProgram.vcxproj`)
3. `ToolchainDiscovery::probe()` → uygun araç zinciri; `BuildPlanner` → derleme komutu
4. `BuildRunner::run(...)` → gerçek derleme, tanıların ayrıştırılması
5. istenirse `runAndCaptureOutput(exe)` → program çıktısı

Durum çubuğu, **Çıktı** paneli ve (başarıda) Explorer'ı açma bu akışın sonunda yapılır.

### 8.6 Araç zinciri keşfi

`ToolchainDiscovery` sırayla dener ve **en uygun** sürümü seçer:

| Aile | Ne arar | Ne yapar |
|---|---|---|
| MSVC | `vswhere` ile `cl.exe` + `vcvars64.bat` + Windows SDK | C++ derleme, MSBuild yolu |
| Clang | `clang++.exe` (PATH / LLVM klasörleri) | alternatif C++ derleyici |
| GCC/MinGW | `g++.exe` | alternatif C++ derleyici |
| .NET | `dotnet --version` | C# derleme (`dotnet build`) |
| Roslyn | SDK içindeki `csc.dll` | raporda listelenir |
| Derleme sistemi | MSBuild, CMake, Ninja | C++ için proje dosyası yolu |
| Paket yönetimi | NuGet önbelleği, vcpkg kökü | paket ekleme hazırlığı |

Derleme planı iki yoldan kurulur:

- **C#** → `dotnet build <proje>.csproj -c Release` (çıktı: `bin/Release/net9.0/<ad>.exe`)
- **C++** → `cl.exe` + `vcvars` (hızlı, MSBuild proje dosyası gerekmez) veya MSBuild + `.vcxproj`

### 8.7 C ABI yüzeyi (Api.h)

`gpcore`, C++ dışındaki tüketiciler için düz bir C arayüzü sunar (`native/include/gpi/Api.h`).
Tüm dizgiler UTF-8'dir ve çağıran tarafça `gp_free()` ile serbest bırakılır. C# tarafında P/Invoke
kullanımı için tasarlanmıştır:

```c
char* gp_generate_code(const char* graphJson, const char* optionsJson);  /* {ok, code, errors[], mapping[]} */
char* gp_graph_analyze(const char* graphJson);                          /* {ok, errors[], warnings[], order[]} */
char* gp_catalog_search(const char* query, const char* library);       /* JSON dizisi */
char* gp_probe_toolchain(void);                                         /* ToolchainReport JSON */
char* gp_build(const char* planJson);                                   /* BuildResult JSON */
gp_free(/* yukarıdakilerden herhangi biri */);
```

---

## 9. Bilinen sınırlamalar

Bunlar bilinçli olarak ertelenmiş veya bilinen davranışlardır; hata olarak gizlenmez.

| # | Konu | Durum / neden |
|---|---|---|
| 1 | **Kod → görsel yok** | Dönüşüm tek yönlüdür. Kod paneli salt okunur bir önizlemedir; kod yazmak görseli değiştirmez. Bu en büyük eksik özelliktir. |
| 2 | C++'ta `str.split` / `str.join` / `str.replace` | Bu düğümler `gpi::str::*` yardımcılarını çağırır; yardımcılar `gpi_runtime.h` içinde **var** ancak üretilen `main.cpp` bu başlığı **dahil etmiyor**. C# tarafı sorunsuz. |
| 3 | C++ büyük/küçük harf ve Türkçe | `std::toupper` yalnızca ASCII harfleri dönüştürür: `MERHABA, GÖRSEL DÜNYA!` yerine `MERHABA, GöRSEL DüNYA!` üretilir. C# `ToUpper()` kültür duyarlıdır ve doğru sonucu verir. |
| 4 | Konsol çıktısında Türkçe karakter | `Console.WriteLine` konsola UTF-8 yazar; eski kod sayfasında ekranda `Ã¶` gibi görünebilir. Dosyaya yazılan baytlar doğrudur. |
| 5 | Menü kısayolları | Menülerde `F5`, `F6`, `Ctrl+Z`, `Ctrl+Y` **etiket olarak** görünür; tuş olayı henüz bağlanmamıştır, şimdilik menüden tıklanır. |
| 6 | Tuval koordinatları | Pan, grafın **sol üst köşesine** sabitlenir; bu nedenle graf koordinatları ≥ 0 tutulmalıdır (negatif düğümler "Sığdır" sonrası kaybolabilir). |
| 7 | Katalog araması | Yalnızca yerleşik 88 düğün aranır. Yansıtılmış API düğümleri için altyapı hazır (`NodeCatalog::makeReflectedType`), ancak C# yansıtma servisi (`Gpi.Sidecar`) henüz yoktur. |
| 8 | Arayüz platformu | `Gpi.Shell` yalnızca Windows'ta derlenir. Çekirdek (`gpcore`) platformdan bağımsızdır. |

---

## 10. Sorun giderme

| Belirti | Sebep | Çözüm |
|---|---|---|
| `cmake --build build` derlemiyor, `build\Debug` yok | CMake önbelleği `build/` kökünde | `cmake -S . -B build` sonrası `--build build --config Debug` kullan |
| Arayüz açılıyor ama **boş** / sadece arka plan | Varsayılan font, ikon fontu olarak seçilmiş olabilir | `main.cpp` içinde font **sırası** önemlidir: önce `segoeui.ttf`, sonra `consola.ttf`; `io.FontDefault` açıkça atanır |
| "Missing PopFont" assertion | `PopFont`, `EndChild`'dan sonra çağrılmış | Panel kodlarında `PopFont` her zaman `EndChild`'dan önce olmalı |
| Pencere hiç çizmiyor | GPU'ya geometri gitmiyor | `Gpi.Shell.exe /probe build\probe.txt 40` → raporda köşe sayısı ve `ImGui hata gunlugu` bölümüne bak |
| Derleme başlıyor ama takılıyor | Araç zinciri keşfi uzun sürebilir | `/trace` ile `build/startup.log` dosyasına bak |
| `Araç Zinciri yok` hatası | MSVC/SDK bulunamadı | Visual Studio'ya "Desktop development with C++" yükünü kur; "Araç Zinciri" panelinden keşfi tazele |
| C# üretimi başarısız, `NETSDK1045` | Hedef çerçeve ile SDK uyuşmuyor | .NET 9 SDK kur ya da `ProjectOptions::framework` değerini SDK ile eşle |
| Üretilen C++ kodu derlenmiyor | `#include <cctype>` gibi başlıklar emit sırasında toplanır; sorun genelde eksik bağımlılık | Üretilen `main.cpp`'yi açıp hata satırına bak; düğümün `cppHeader` alanını güncelle |
| Explorer penceresi açılmıyor | `ShellExecuteW` engellenmiş olabilir | Menüden **Derle → Derlemede klasörü aç** işaretini kaldır; exe yolu Çözüm Gezgini'nde görünür |
| Konsolda Türkçe karakterler bozuk | Kod sayfası | Dosyaya yazılan kod UTF-8'dir; sorun yalnızca konsol ekranındadır |

---

## 11. Katkı rehberi: yeni düğüm nasıl eklenir?

**1) Tanımı yaz.** `native/src/BuiltinNodes.cpp` (matematik/metin) veya
`native/src/BuiltinFlow.cpp` (akış/koleksiyon/G/Ç/değişken) dosyasında bir fabrika fonksiyonu ekle:

```cpp
NodeType makeTrimLeft() {
  NodeType n = unary(
      "str.trimleft", "Baştaki Boşlukları Kırp", "\uE8F1", t::String(), t::String(),
      [](EmitContext& c, const std::string& v) {
        if (c.isCSharp()) return v + ".TrimStart()";
        return "([](std::string s){ std::size_t b = s.find_first_not_of(\" \\t\\n\\r\"); "
               "return b == std::string::npos ? std::string() : s.substr(b); })(" + v + ")";
      },
      {}, "<string>");
  n.category = "Metin";
  n.description = "Metnin başındaki boşlukları siler.";
  return n;
}
```

**2) Portları tanımla.** `in()` giriş, `out()` çıkış portu üretir. Varsayılan literal'ı olmayan port
`required = true` olur (bağlanmazsa hata üretir):

```cpp
n.inputs  = { in("Value", t::String(), "\"\"", "Değer") };
n.outputs = { out("Result", t::String(), "Sonuç") };
```

**3) İki dil için emit yaz.** `emitExpr` ifade döndürür (saf düğümlerde gömülür), `emit` ise satır yazar.
`c.isCSharp()` ile diller arasında seçim yap.

**4) Gerekli başlık/using bildir.** C++ için `n.cppHeader = "<cctype>";`, C# için
`n.csUsings = {"System"};`.

**5) Kataloğa kaydet.** `native/src/Catalog.cpp` içindeki `loadBuiltins()` fonksiyonuna bir satır:

```cpp
registerType(makeTrimLeft());
```

**6) Doğrula.**

```powershell
cmake --build build --config Debug
.\build\Debug\gpcli.exe catalog kirp        # Türkçe arama normalize eder
.\build\Debug\gp_tests.exe
.\build\Debug\gpcli.exe demo cpp            # C++ hedefinde gerçekten derlenip çalıştığını gör
```

> **Türkçe arama:** `Catalog.cpp` içindeki `foldForSearch()` `ç ğ ı ö ş ü` harflerini ASCII'ye katlar;
> kullanıcı "kırp" yazdığında "kirp" düğümü bulunur.

**Kod stili**: 2 boşluk girinti, 100 karakter sütun, `snake_case` fonksiyon/değişken,
`PascalCase` tip adı, yorumlar **Türkçe**. MSVC için `/W4 /permissive-` uyarısız geçmeli.

---

## 12. Proje yapısı ve dosya sorumlulukları

```
.
├── CMakeLists.txt                 Tek dosyalık derleme tanımı (gpcore, Gpi.Shell, gpcli, gp_tests)
├── .gitignore                     Üretim, log ve IDE dosyaları hariç tutulur
├── README.md                      Bu belge
│
├── native/                        ── ÇEKİRDEK (gpcore): arayüzden bağımsız ──
│   ├── include/gpi/
│   │   ├── Api.h                  C ABI: P/Invoke için dışa açılan düz C arayüzü
│   │   ├── Codegen.hpp            ProjectOptions, GenResult, CodeGenerator, ProjectGenerator
│   │   ├── Emit.hpp               EmitContext: satır/başlık/eşleme üretim bağlamı
│   │   ├── Graph.hpp              Graph, NodeInstance, Edge, PortKey, analyze()
│   │   ├── Json.hpp               Bağımlılıksız JSON okuma/yazma
│   │   ├── NodeCatalog.hpp        NodeType, PortDef, arama, kategoriler
│   │   ├── Serialization.hpp      Graf JSON'u, buildStringDemo()
│   │   ├── Toolchain.hpp          Araç zinciri keşfi, BuildPlan, BuildRunner, Diagnostic
│   │   └── Types.hpp              TypeRef/TypeKind, dönüşüm denetimi, tip adlandırma
│   ├── src/
│   │   ├── Api.cpp                C ABI uygulaması
│   │   ├── Build.cpp              Süreç başlatma, boru hattı, çıktı/timeout yakalama
│   │   ├── BuiltinFlow.cpp        Akış, karşılaştırma, mantık, koleksiyon, değişken, G/Ç düğümleri
│   │   ├── BuiltinNodes.cpp       Matematik ve metin düğümleri
│   │   ├── Catalog.cpp            loadBuiltins(), Türkçe duyarlı arama, yansıtılan düğüm
│   │   ├── Codegen.cpp            Kontrol akışı üretimi, satır eşlemesi, fonksiyon üretimi
│   │   ├── Emit.cpp               EmitContext uygulaması, girinti/çizgi takibi
│   │   ├── Graph.cpp              Doğrulama, topolojik sıra, düğüm/port tanıları
│   │   ├── Json.cpp               Ayrıştırıcı ve seri hâleyicisi
│   │   ├── ProjectGen.cpp         .csproj, .vcxproj, gpi_runtime.h, P/Invoke iskele dosyaları
│   │   ├── Serialization.cpp      Graf JSON serileştirme, örnek dize programı
│   │   ├── Toolchain.cpp          MSVC/.NET/Clang/GCC/MSBuild/vcpkg keşfi
│   │   └── Types.cpp              Tip çözümleme, C#/C++ tip adları, dönüşüm kuralları
│   └── tests/
│       ├── gp_cli.cpp             gpcli: probe | catalog | gen | demo | selftest
│       └── gp_tests.cpp           66 birim testi
│
├── shell/                         ── ARAYÜZ (Gpi.Shell): Windows + ImGui ──
│   ├── include/shell/
│   │   ├── App.hpp                Uygulama sınıfı: menüler, paneller, durum bayrakları
│   │   ├── BuildService.hpp       Üret → derle → çalıştır iş parçacığı
│   │   ├── CrashHandler.hpp       SEH yakalayıcı, minidump yazımı
│   │   ├── EditorState.hpp        Graf, seçim, geri al/yinele, pan/zoom, dosya
│   │   ├── FileDialog.hpp         Windows ortak dosya iletişim kutusu
│   │   ├── LiveAnalyzer.hpp       Arka plan denetimi + kod önizlemesi
│   │   ├── Panel.hpp              Panel arayüzü
│   │   ├── Platform.hpp           Win32 pencere + D3D11 aygıtı
│   │   ├── Theme.hpp              Koyu tema, UI/kod fontu erişimi
│   │   └── panels/*.hpp           Panel başlıkları
│   └── src/
│       ├── main.cpp               wWinMain: pencere, fontlar, ImGui/DX11, döngü, /selftest /probe /trace
│       ├── App.cpp                Yerleşim, menüler, panel çizimi, F5/F6 akışı, örnek program
│       ├── Platform.cpp           Pencere sınıfı, swapchain, resize
│       ├── EditorState.cpp        Graf düzenleme, işlem (undo) yığını, JSON kaydet/yükle
│       ├── BuildService.cpp       İş parçacığı: üret, yaz, derle, çalıştır, Explorer'ı aç
│       ├── LiveAnalyzer.cpp       Kuyruk + iş parçacığı, snapshot
│       ├── Theme.cpp              Koyu tema ayarları
│       ├── FileDialog.cpp         IFileDialog sarmalayıcı
│       ├── CrashHandler.cpp       Yapılandırılmış özel durum işleme
│       └── panels/
│           ├── CanvasPanel.cpp    Izgara, düğümler, kablolar, portlar, pan/zoom, seçim, hata vurgusu
│           ├── CatalogPanel.cpp   Kategori listesi + Türkçe arama
│           ├── DetailsPanel.cpp   Seçili düğümün port/özellik düzenleyicisi
│           ├── CodePanel.cpp      Satır numaralı, renklendirilmiş, kaydırmalı kod görünümü
│           ├── OutputPanel.cpp    Derleme günlüğü, tanılar, program çıktısı
│           ├── SolutionPanel.cpp  Üretilen proje dosyaları ağacı
│           └── ToolchainPanel.cpp Keşfedilen araç zinciri raporu
│
└── third_party/imgui/             Dear ImGui (MIT) — kaynak kodla birlikte gelir
```

---

## 13. Düğüm kataloğu (tam liste)

**88 düğüm tipi**, 10 kategori. `gpcli catalog` komutuyla aynı liste dökülür.

### Değer (4)

| Kimlik | Görünen ad | Not |
|---|---|---|
| `lit.number` | Tam Sayı | saf |
| `lit.double` | Ondalık | saf |
| `lit.text` | Metin | saf |
| `lit.bool` | Doğru / Yanlış | saf |

### Matematik (17)

`math.add` Toplama · `math.sub` Çıkarma · `math.mul` Çarpma · `math.div` Bölme · `math.mod` Kalan ·
`math.neg` Ters İşaret · `math.abs` Mutlak Değer · `math.min` En Küçük · `math.max` En Büyük ·
`math.power` Üs Alma · `math.sqrt` Karekök · `math.sin` Sinüs · `math.cos` Kosinüs ·
`math.floor` Yuvarla Aşağı · `math.ceil` Yuvarla Yukarı · `math.round` Yuvarla ·
`math.random` Rastgele Sayı

### Metin (16)

`str.concat` Birleştir · `str.format` Biçimlendir · `str.substring` Alt Metin ·
`str.upper` Büyük Harfe Çevir · `str.lower` Küçük Harfe Çevir · `str.trim` Kırp ·
`str.split` Böl · `str.join` Birleştir (Liste) · `str.contains` İçeriyor mu ·
`str.replace` Değiştir · `str.indexof` Konum Bul · `str.length` Uzunluk · `str.lines` Satırlara Böl

### Dönüşüm (2)

`str.toint` Metinden Sayıya · `str.fromint` Sayıdan Metne

### Akış (11)

`flow.if` Eğer / Değilse · `flow.while` Koşul Sağlandıkça · `flow.forrange` Sayılarla Tekrarla ·
`flow.branch` Değere Göre Dallan · `flow.foreach.int/string/double` Listedeki Her … ·
`flow.break` Döngüden Çık · `flow.continue` Sonraki Yineleme · `flow.exit` Programdan Çık ·
`flow.comment` Açıklama

### Karşılaştırma (3)

`flow.compare.int` Karşılaştır (Sayı) · `flow.compare.float` Karşılaştır (Ondalık) ·
`flow.compare.string` Karşılaştır (Metin)

### Mantık (3)

`flow.and` Ve · `flow.or` Veya · `flow.not` Değil

### Koleksiyon (19)

Liste (`:int`, `:string`, `:double` varyantları): `list.new` · `list.add` · `list.get` · `list.set` · `list.count`
Sözlük: `dict.new.int` · `dict.get.int` · `dict.set.int` · `dict.has.int`

### Giriş / Çıkış (3)

`io.print` Ekrana Yaz · `io.readline` Satır Oku · `io.delay` Bekle

### Değişken (12)

`var.make.int/string/double/bool` Tanımla · `var.get.*` Oku · `var.set.*` Yaz

### Tanım (2)

`def.class` Sınıf Tanımı · `struct.new` Nesne Oluştur

---

## 14. Üretilen dosyalar ve çalışma dizinleri

IDE, üretimini **her derlemede** `build/out-shell/` altına temiz şekilde yazar:

```
build/
├── out-shell/                          ← C# hedefi
│   ├── Program.g.cs                    üretilen ana kaynak
│   ├── GpiProgram.csproj               .NET 9, konsol uygulaması
│   └── bin/Release/net9.0/GpiProgram.exe      derlenmiş program
│
└── out-shell/native/                   ← C++ hedefi
    ├── main.cpp                        üretilen ana kaynak
    ├── gpi_runtime.h                   C++ yardımcı işlevler (gpi::str::split vb.)
    ├── gpi_native.h                    P/Invoke iskeleleri
    └── GpiProgram.exe                  cl.exe çıktısı
```

Başarılı derlemeden sonra bu klasör Explorer'da açılır (exe seçili). `gpcli` ise kendi verdiğiniz
klasöre yazar (`out-cs`, `out-cpp`, `build/out-demo` …).

**Üretilen C# biçimi** (varsayılan seçenekler: `net9.0`, `Exe`, yorumlar açık, saf düğümler gömülü):

```csharp
using System;
using System.Collections.Generic;

namespace GpiProgram
{
    static class Program
    {
        static void Main()
        {
            // --- Metin (lit.text) ---
            string n0_value = "Merhaba";
            // --- Metin (lit.text) ---
            string n1_value = ", dünya!";
            // --- Birleştir (str.concat) ---
            string n2_result = (n0_value + n1_value);
            // --- Büyük Harfe Çevir (str.upper) ---
            string n3_result = n2_result.ToUpper();
            // --- Ekrana Yaz (io.print) ---
            Console.WriteLine(n3_result);
        }
    }
}
```

**Üretilen C++ biçimi:**

```cpp
#include <iostream>
#include <string>
#include <cctype>

int main()
{
    // --- Metin (lit.text) ---
    const std::string n0_value = "Merhaba";
    // --- Metin (lit.text) ---
    const std::string n1_value = ", dünya!";
    // --- Birleştir (str.concat) ---
    const std::string n2_result = (n0_value + n1_value);
    // --- Büyük Harfe Çevir (str.upper) ---
    const std::string n3_result = ([](std::string s) { for (auto& ch : s) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch))); return s; })(n2_result);
    // --- Ekrana Yaz (io.print) ---
    std::cout << n3_result << std::endl;
    return 0;
}
```

---

## 15. Tasarım kararları

**Neden C++20 ve CMake?** Çekirdeğin dil ve araç zinciri keşfi, MSVC/vcvars, NuGet ve dosya sistemi
üzerinde doğrudan çalışması gerekiyor. Tek bir C++ çekirdek sayesinde hem C++ hem C# hedefleri
aynı koddan üretilebiliyor, hem de C# tarafına P/Invoke ile açılabiliyor.

**Neden ImGui + DirectX 11?** Tek bir "anlık görüntü" arayüzü, geleneksel widget/olay döngüsü
yoktur. Bu da iki büyük kazanç sağlar: (a) her şey `App::draw()` içinde çizildiği için
`/selftest` ile **penceresiz** tüm düzen sınanabilir, (b) `/probe` ile "ekran boş mu?" sorusu
sayısal olarak cevaplanabilir. Koyu tema, panel dock'u, sekmeler ve Türkçe metin desteği ImGui'nin
doğal özellikleridir.

**Neden exec (akış) portları ayrı?** Değer portları "veri", exec portları "ne zaman" taşır. Bu ayrım,
saf düğümlerin ifade olarak gömülmesini ve üretilen kodun okunabilir kalmasını sağlar. Ayrıca birçok
graf motorunun kullandığı modeldir ve C++/C#'a doğrudan karşılık gelir.

**Neden her düğüm iki dil için ayrı kod yazıyor?** "Bir IR'den iki dili derleyen ortak bir dil"
yazmak, iki dilin gerçek farklarını (koleksiyon sözdizimi, string API'leri, tür sistemi) gizlerdi.
Bunun yerine her düğüm, `c.isCSharp()` ile küçük bir dallanmayla iki çıktı verir: emit
lambda'ları kısa ve gözden geçirilebilir kalır, üretilen kod okunur olur.

**Neden anlık denetim ayrı bir iş parçacığında?** Denetim + üretim mikrosaniyeler sürse de, graf
büyüdükçe maliyet artar. Arka plana alındığında tuval hiç takılmaz, ayrıca `revision` sayacı
gereksiz tekrarları da eler.

**Neden kod paneli salt okunur?** Bu, bilinçli bir sınırdır: iki yönlü düzenleme (kodu okuyup
grafı güncellemek) gerçek bir ayrıştırıcı ve dayanıklı bir birleştirme stratejisi gerektirir. Yanlış
bir eşleştirme kullanıcının programını sessizce bozabileceği için önce tek yönlü akış sağlam olsun
istendi. Kod paneli ileride düzenlenebilir hale getirilmiş tasarlanmıştır (satır eşlemesi
`nodeLines`/`mapping` ile hazırdır).

---

## 16. Lisans ve teşekkürler

- **Bu depodaki kaynak kod** MIT lisansıyla sunulmak üzere hazırlanmıştır (yayımlarken bir
  `LICENSE` dosyası ekleyin).
- **Dear ImGui**, MIT lisanslıdır ve kaynak kodla birlikte gelir: `third_party/imgui/LICENSE.txt`.
- **Microsoft .NET / MSBuild / Windows SDK** yalnızca çalışma zamanı araç zinciri olarak
  kullanılır; depoya dâhil değildir.
- Aşağıdaki projelere ilham verdi:
  - [Dear ImGui](https://github.com/ocornut/imgui) — anlık görüntü arayüz kütüphanesi
  - Blueprint / Unreal Engine — görsel programlama ve akış kontrolü modeli
  - Node-RED, Grasshopper — düğüm grafi düzenleme arayüzleri

---

## Yol Haritası (fikirler)

- [ ] **Kod → görsel**: kod panelini düzenlenebilir yapmak, satır eşlemesi üzerinden geri yükleme
- [ ] C++ hedefinde `gpi_runtime.h`'yi `main.cpp`'ye dahil ederek `split`/`join`/`replace` düzeltmesi
- [ ] Türkçe büyük/küçük harf için `std::toupper` yerine UTF-8 destekli yardımcı
- [ ] Alt graf → yeniden kullanılabilir fonksiyon düğümü (`generateFunction` altyapısı hazır)
- [ ] Yansıtılmış API düğümleri için `Gpi.Sidecar` (C# yansıma servisi)
- [ ] Yeni düğüm kütüphanesi: dosyadan yüklenen JSON düğüm tanımları
- [ ] Sürükle-bırak dosya/asset desteği, proje şablonları