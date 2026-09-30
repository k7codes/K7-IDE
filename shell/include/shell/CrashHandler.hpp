// Shell: yakalanmamış istisna / erişim ihlali yakalayıcı
#pragma once

namespace shell {

// main'in en başında çağrılır. Yakalanmamış bir hata olursa
// uygulama dizinine "<ad>-crash.txt" yazılır (sembolleştirilmiş yığın).
// Ayrıca debug CRT assertion'ları diyalog yerine stderr'e yönlendirilir.
void installCrashHandler();

}  // namespace shell
