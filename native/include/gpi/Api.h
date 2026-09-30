/* Gözle Programlama IDE - C++ çekirdeğin C ABI'si
 * C# tarafı bu başlığı P/Invoke ile kullanır. Tüm dizgeler UTF-8'dir ve
 * çağıran taraf tarafından gp_free ile serbest bırakılmalıdır.
 */
#ifndef GPI_API_H
#define GPI_API_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#  define GPI_API __declspec(dllexport)
#  define GPI_CALL __stdcall
#else
#  define GPI_API __attribute__((visibility("default")))
#  define GPI_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Bellek yönetimi                                                     */
/* ------------------------------------------------------------------ */

/** C++ tarafından ayrılmış bir dizgiyi serbest bırakır. */
GPI_API void GPI_CALL gp_free(char* ptr);

/** Sürüm bilgisi: "1.0.0" */
GPI_API const char* GPI_CALL gp_version(void);

/** Son hata mesajı (iş parçacığına özel) */
GPI_API const char* GPI_CALL gp_last_error(void);

/* ------------------------------------------------------------------ */
/* Günlük geri çağrısı                                                  */
/* ------------------------------------------------------------------ */

typedef enum gp_log_level {
  GP_LOG_DEBUG = 0,
  GP_LOG_INFO = 1,
  GP_LOG_WARN = 2,
  GP_LOG_ERROR = 3
} gp_log_level;

/** (level, utf8_message, user_data) */
typedef void(GPI_CALL* gp_log_fn)(int level, const char* message, void* user_data);
GPI_API void GPI_CALL gp_set_logger(gp_log_fn fn, void* user_data);

/* ------------------------------------------------------------------ */
/* JSON yardımcıları                                                   */
/* ------------------------------------------------------------------ */

/** Graf JSON'unu ayrıştırır. Hata halinde null döner. */
GPI_API char* GPI_CALL gp_graph_from_json(const char* json);

/** Proje seçenekleri JSON'unu ayrıştırır. */
GPI_API char* GPI_CALL gp_project_options_from_json(const char* json);

/* ------------------------------------------------------------------ */
/* Düğüm kataloğu                                                      */
/* ------------------------------------------------------------------ */

/** Tüm düğüm tiplerinin JSON dizisi */
GPI_API char* GPI_CALL gp_catalog_json(void);

/** Kategorilerin JSON dizisi */
GPI_API char* GPI_CALL gp_catalog_categories_json(void);

/** Tek düğüm tipinin JSON'u (bulunamazsa null) */
GPI_API char* GPI_CALL gp_catalog_type_json(const char* typeId);

/** Serbest metin araması. Sonuçlar JSON dizisi. */
GPI_API char* GPI_CALL gp_catalog_search(const char* query, const char* library);

/* ------------------------------------------------------------------ */
/* Tip sistemi                                                         */
/* ------------------------------------------------------------------ */

/** Verilen JSON tip tanımını C#/C++ adlarına çevirir. */
GPI_API char* GPI_CALL gp_type_resolve(const char* typeJson);

/** Dönüşüm denetimi: 1=uygun, 0=uygun değil. */
GPI_API int GPI_CALL gp_type_assignable(const char* fromJson, const char* toJson);

/* ------------------------------------------------------------------ */
/* Analiz ve doğrulama                                                 */
/* ------------------------------------------------------------------ */

/** Grafı doğrular. JSON: {ok, errors[], warnings[], order[]} */
GPI_API char* GPI_CALL gp_graph_analyze(const char* graphJson);

/* ------------------------------------------------------------------ */
/* Kod üretimi                                                         */
/* ------------------------------------------------------------------ */

/** JSON: {ok, code, errors[], warnings[], usings[], mapping[]} */
GPI_API char* GPI_CALL gp_generate_code(const char* graphJson, const char* optionsJson);

/** Proje dosyalarını üretir. JSON: {files:[{path, content}], notes:[]} */
GPI_API char* GPI_CALL gp_generate_project(const char* optionsJson);

/** Alt grafdan fonksiyon üretir (yeniden kullanılabilir düğümler için). */
GPI_API char* GPI_CALL gp_generate_function(const char* graphJson, const char* subgraphJson,
                                            const char* optionsJson, const char* fnName);

/* ------------------------------------------------------------------ */
/* Araç zinciri                                                        */
/* ------------------------------------------------------------------ */

/** Derleyici/derleme sistemi keşfi. JSON ToolchainReport */
GPI_API char* GPI_CALL gp_probe_toolchain(void);

/** Derleme planını hazırlar. JSON: {valid, error, command:[], shellPrefix, ...} */
GPI_API char* GPI_CALL gp_plan_build(const char* optionsJson);

/* ------------------------------------------------------------------ */
/* Derleme / çalıştırma                                                */
/* ------------------------------------------------------------------ */

/**
 * Derlemeyi gerçekleştirir. İlerleme satırları logger'a düşer.
 * JSON BuildResult döner.
 */
GPI_API char* GPI_CALL gp_build(const char* planJson);

/** Yürütülebilir dosyayı çalıştırır, stdout/stderr toplar. JSON BuildResult */
GPI_API char* GPI_CALL gp_run_executable(const char* exePath, const char* argsJson,
                                         const char* workingDir, int timeoutSeconds);

/* ------------------------------------------------------------------ */
/* Paket yönetimi (NuGet / vcpkg)                                      */
/* ------------------------------------------------------------------ */

/** NuGet araması. JSON: {id, version, description, authors, downloads, iconUrl} */
GPI_API char* GPI_CALL gp_nuget_search(const char* query, int take, int skip);

/** Paketi projeye ekler. */
GPI_API int GPI_CALL gp_nuget_add_package(const char* projectDir, const char* projectFile,
                                          const char* packageId, const char* version);

/** vcpkg ara. Kurulu değilse null. */
GPI_API const char* GPI_CALL gp_vcpkg_root(void);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* GPI_API_H */
