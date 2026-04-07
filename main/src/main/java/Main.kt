import org.luckypray.dexkit.DexKitBridge
import java.io.File
import java.lang.reflect.Modifier
import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit
import java.util.concurrent.TimeoutException
import kotlin.concurrent.thread

val isWindows = System.getProperty("os.name")
    .lowercase()
    .contains("windows")

@Suppress("UnsafeDynamicallyLoadedCode")
fun loadLibrary(name: String) {
    try {
        System.loadLibrary(if (isWindows) "lib$name" else name)
    } catch (e: UnsatisfiedLinkError) {
        val libraryPath = File("main/build/library")
        libraryPath.listFiles()?.forEach {
            if (it.name.startsWith("lib$name")) {
                System.load(it.absolutePath)
            }
        }
    }
}

fun main() {
    loadLibrary("dexkit")
    println("current work dir: ${File("").absolutePath}")
//    val file = File("D:\\Project\\Android\\ArscEditor\\lib\\build\\out-sign.apk")
    val file = File("D:\\Project\\Android\\WeWa\\repack\\origin\\origin_random_2.26.1.76.apk")
    if (!file.exists()) {
        println("apk not found")
        return
    }
    doSearch(file.absolutePath)
}

fun doSearch(path: String) {
    val createTime = System.currentTimeMillis()
    var releaseTime = 0L
    DexKitBridge.create(path).use { bridge ->
//        bridge.initFullCache()
        println("create use time: ${System.currentTimeMillis() - createTime}ms")
        val startTime = System.currentTimeMillis()
        search(bridge)
        println("find use time: ${System.currentTimeMillis() - startTime}ms")
        releaseTime = System.currentTimeMillis()
    }
    println("release use time: ${System.currentTimeMillis() - releaseTime}ms")
}

fun search(bridge: DexKitBridge) {
    bridge.setThreadNum(8)
    val workers = 20
    val iterationsPerWorker = 1
    val start = CountDownLatch(1)
    val executor = Executors.newFixedThreadPool(workers)
    try {
        val futures = (0 until workers).map { workerIndex ->
            executor.submit<Unit> {
                val threadName = Thread.currentThread().name
                println("worker-$workerIndex start on $threadName")
                start.await(10, TimeUnit.SECONDS)
                repeat(iterationsPerWorker) { iteration ->
                    println("worker-$workerIndex iteration-$iteration before query on $threadName")
                    val result = bridge.findMethod {
                        matcher {
                            addInvoke("LX/Db9;-><init>(Ljava/lang/Object;Ljava/lang/Object;I)V")
                            usingNumbers(0x1f)
                        }
                    }
                    println("worker-$workerIndex iteration-$iteration result on $threadName: $result")
                }
                println("worker-$workerIndex end on $threadName")
            }
        }
        start.countDown()
        futures.forEachIndexed { index, future ->
            try {
                future.get(60, TimeUnit.SECONDS)
            } catch (e: TimeoutException) {
                println("future-$index timeout")
                dumpAllThreads()
                throw e
            }
        }
    } finally {
        executor.shutdownNow()
    }
}

fun dumpAllThreads() {
    println("=".repeat(80))
    println("thread dump begin")
    Thread.getAllStackTraces().forEach { (thread, stack) ->
        println("[${thread.name}] state=${thread.state}")
        stack.forEach { println("    at $it") }
        println()
    }
    println("thread dump end")
    println("=".repeat(80))
}


