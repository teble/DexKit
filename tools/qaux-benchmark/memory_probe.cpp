// Benchmark-only process memory observations, independent of the DexKit engine.
#include <jni.h>
#include <sys/resource.h>
#include <mach/mach.h>
#include <malloc/malloc.h>
#include <libproc.h>
#include <unistd.h>

extern "C" JNIEXPORT jlongArray JNICALL
Java_QueryReplay_memorySnapshot(JNIEnv *env, jclass) {
    mach_task_basic_info_data_t info{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    const auto status = task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                                  reinterpret_cast<task_info_t>(&info), &count);
    rusage usage{};
    const auto usage_status = getrusage(RUSAGE_SELF, &usage);
    malloc_statistics_t allocations{};
    malloc_zone_statistics(nullptr, &allocations);
    rusage_info_v4 footprint{};
    const auto footprint_status = proc_pid_rusage(getpid(), RUSAGE_INFO_V4,
            reinterpret_cast<rusage_info_t *>(&footprint));
    const jlong values[] = {
        status == KERN_SUCCESS ? static_cast<jlong>(info.resident_size) : -1,
        usage_status == 0 ? static_cast<jlong>(usage.ru_maxrss) : -1,
        static_cast<jlong>(allocations.size_in_use),
        static_cast<jlong>(allocations.size_allocated),
        static_cast<jlong>(allocations.blocks_in_use),
        footprint_status == 0 ? static_cast<jlong>(footprint.ri_phys_footprint) : -1,
        footprint_status == 0 ? static_cast<jlong>(footprint.ri_lifetime_max_phys_footprint) : -1,
    };
    auto result = env->NewLongArray(7);
    if (result != nullptr) env->SetLongArrayRegion(result, 0, 7, values);
    return result;
}
