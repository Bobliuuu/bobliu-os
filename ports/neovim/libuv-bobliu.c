/* The libuv platform functions that need OS support. bobliu has no /proc and no sysinfo call, so
   most of these report "unknown" (zero) instead of failing, which is what callers cope with best.
   Built into libuv.a by ports/neovim/build-libuv.sh. */
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <uv.h>

int uv_exepath(char* buffer, size_t* size) {
  (void)buffer; (void)size;
  return UV_ENOSYS;                       /* no /proc/self/exe: callers fall back to argv[0] + PATH */
}

int uv_uptime(double* uptime) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    return UV_EIO;
  *uptime = (double)ts.tv_sec + ts.tv_nsec / 1e9;
  return 0;
}

int uv_resident_set_memory(size_t* rss) { *rss = 0; return 0; }
void uv_loadavg(double avg[3]) { avg[0] = avg[1] = avg[2] = 0; }
uint64_t uv_get_free_memory(void) { return 0; }
uint64_t uv_get_total_memory(void) { return 0; }
uint64_t uv_get_constrained_memory(void) { return 0; }
uint64_t uv_get_available_memory(void) { return 0; }

int uv_cpu_info(uv_cpu_info_t** cpu_infos, int* count) {
  *cpu_infos = NULL;
  *count = 0;
  return UV_ENOSYS;
}

int uv_interface_addresses(uv_interface_address_t** addresses, int* count) {
  *addresses = NULL;
  *count = 0;
  return 0;                               /* no network interfaces */
}

void uv_free_interface_addresses(uv_interface_address_t* addresses, int count) {
  (void)count;
  free(addresses);
}
