package org.luckypray.dexkit

import org.junit.Test
import org.luckypray.dexkit.annotations.DexKitExperimentalApi
import org.luckypray.dexkit.query.enums.StringMatchType
import java.io.File
import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicReference


class UnitTest {

    companion object {

        private val demoApkPath: String
        private var bridge: DexKitBridge
        private val tokenField = DexKitBridge::class.java.getDeclaredField("token").apply {
            isAccessible = true
        }
        private val nativeGetMethodUsingStringsMethod = DexKitBridge::class.java.getDeclaredMethod(
            "nativeGetMethodUsingStrings",
            Long::class.javaPrimitiveType!!,
            Long::class.javaPrimitiveType!!
        ).apply {
            isAccessible = true
        }
        private val nativeGetMethodOpCodesMethod = DexKitBridge::class.java.getDeclaredMethod(
            "nativeGetMethodOpCodes",
            Long::class.javaPrimitiveType!!,
            Long::class.javaPrimitiveType!!
        ).apply {
            isAccessible = true
        }

        init {
            loadLibrary("dexkit")
            val path = System.getProperty("apk.path")
            val demoApk = File(path, "demo.apk")
            demoApkPath = demoApk.absolutePath
            bridge = DexKitBridge.create(demoApk.absolutePath)
        }
    }

    private fun getBridgeToken(target: DexKitBridge): Long {
        return tokenField.getLong(target)
    }

    private fun nativeGetMethodUsingStrings(token: Long, encodeId: Long): List<String> {
        @Suppress("UNCHECKED_CAST")
        return (nativeGetMethodUsingStringsMethod.invoke(null, token, encodeId) as Array<String>).toList()
    }

    private fun nativeGetMethodOpCodes(token: Long, encodeId: Long): List<Int> {
        return (nativeGetMethodOpCodesMethod.invoke(null, token, encodeId) as IntArray).toList()
    }

    @Test
    fun testGetDexNum() {
        assert(bridge.getDexNum() > 0)
    }

    @Test
    fun testPackages() {
        bridge.findClass {
            searchPackages("org.luckypray.dexkit.demo")
            excludePackages("org.luckypray.dexkit.demo.annotations")
        }.forEach {
            println(it.name)
        }
    }

    @Test
    fun testGetParameterNames() {
        bridge.findMethod {
            matcher {
                declaredClass("org.luckypray.dexkit.demo.PlayActivity")
            }
        }.forEach {
            println(it.descriptor)
            println("paramNames: ${it.paramNames?.joinToString(",")}")
        }
    }

    @Test
    fun testAnnotationSearch() {
        val res = bridge.findClass {
            matcher {
                annotations {
                    add {
                        this.type("Router", StringMatchType.EndsWith)
                    }
                }
            }
        }
        println(res.map { it.name })
        assert(res.size == 2)
        res.forEach {
            assert(it.name.startsWith("org.luckypray.dexkit.demo"))
            assert(it.name.endsWith("Activity"))
            assert(it.annotations.size == 1)
            assert(it.annotations.first().typeName == "org.luckypray.dexkit.demo.annotations.Router")
        }
    }

    @Test
    fun testAnnotationForValue() {
        val res = bridge.findClass {
            matcher {
                addAnnotation {
                    type("Router", StringMatchType.EndsWith)
                    addElement {
                        name = "path"
                        value {
                            stringValue("/main", StringMatchType.Equals)
                        }
                    }
                }
            }
        }
        println(res)
        assert(res.size == 1)
        val mainActivity = res.first()
        assert(mainActivity.name == "org.luckypray.dexkit.demo.MainActivity")
        assert(mainActivity.superClass!!.name == "androidx.appcompat.app.AppCompatActivity")
        assert(mainActivity.interfaceCount == 1)
    }

    @Test
    fun testClassFieldsSearch() {
        val res = bridge.findClass {
            matcher {
                fields {
                    addForType("java.lang.String")
                    addForType("android.widget.TextView")
                    addForType("android.os.Handler")
                    count(3)
                }
            }
        }
        println(res)
        assert(res.size == 1)
        val playActivity = res.first()
        assert(playActivity.fields.size == 3)
        assert(playActivity.name == "org.luckypray.dexkit.demo.PlayActivity")
    }

    @Test
    fun testFindClassUsingString() {
        val res = bridge.findClass {
            matcher {
                usingStrings("PlayActivity")
            }
        }
        println(res)
        assert(res.size == 1)
        assert(res.first().name == "org.luckypray.dexkit.demo.PlayActivity")
    }

    @Test
    fun testFindClassUsingStringArray() {
        val res = bridge.findClass {
            matcher {
                usingStrings("PlayActivity", "onClick")
            }
        }
        println(res)
        assert(res.size == 1)
        assert(res.first().name == "org.luckypray.dexkit.demo.PlayActivity")
    }

    @Test
    fun testFindClassSuper() {
        val res = bridge.findClass {
            matcher {
                superClass("androidx.appcompat.app.AppCompatActivity")
            }
        }
        println(res)
        assert(res.size == 2)
        res.map { it.name }.forEach {
            assert(it.startsWith("org.luckypray.dexkit.demo"))
            assert(it.endsWith("Activity"))
        }
    }

    @Test
    fun testFindClassImpl() {
        val res = bridge.findClass {
            searchPackages("org.luckypray.dexkit.demo")
            matcher {
                superClass("AppCompatActivity", StringMatchType.EndsWith)
                interfaces {
                    add("android.view.View\$OnClickListener")
                    count(1)
                }
            }
        }
        println(res)
        assert(res.size == 1)
        assert(res.first().name == "org.luckypray.dexkit.demo.MainActivity")
    }

    @Test
    fun testFindClassImplIgnoreCase() {
        val res = bridge.findClass {
            searchPackages("org.luckypray.dexkit.demo")
            matcher {
                superClass("appcompatactivity", StringMatchType.EndsWith, true)
                interfaces {
                    add("android.view.View\$OnClicklistener", StringMatchType.Equals, true)
                    count(1)
                }
            }
        }
        println(res)
        assert(res.size == 1)
        assert(res.first().name == "org.luckypray.dexkit.demo.MainActivity")
    }

    @Test
    fun testIntNumberSearch() {
        val res = bridge.findMethod {
            excludePackages("org.luckypray.dexkit.demo.hook")
            matcher {
                usingNumbers {
                    add {
                        intValue(114514)
                    }
                }
            }
        }
        println(res)
        assert(res.size == 2)
    }

    @Test
    fun testIntAndFloatNumberSearch() {
        val res = bridge.findMethod {
            excludePackages("org.luckypray.dexkit.demo.hook")
            matcher {
                usingNumbers {
                    add {
                        floatValue(0.987f)
                    }
                    add {
                        intValue(114514)
                    }
                }
            }
        }
        println(res)
        assert(res.size == 1)
    }

    @Test
    fun testGetClassData() {
        val res = bridge.getClassData("Lorg/luckypray/dexkit/demo/MainActivity;")
        assert(res != null)
        assert(res!!.name == "org.luckypray.dexkit.demo.MainActivity")
        res.methods.forEach {
            println(it.descriptor)
        }
    }

    @Test
    fun testGetConstructorData() {
        val res = bridge.getMethodData("Lorg/luckypray/dexkit/demo/MainActivity;-><init>()V")
        assert(res != null)
        assert(res!!.className == "org.luckypray.dexkit.demo.MainActivity")
        assert(res.methodName == "<init>")
        assert(res.methodSign == "()V")
        assert(res.isConstructor)
    }

    @Test
    fun testGetMethodData() {
        val res = bridge.getMethodData("Lorg/luckypray/dexkit/demo/MainActivity;->onClick(Landroid/view/View;)V")
        assert(res != null)
        assert(res!!.className == "org.luckypray.dexkit.demo.MainActivity")
        assert(res.methodName == "onClick")
        assert(res.methodSign == "(Landroid/view/View;)V")
        assert(res.isMethod)
    }

    @Test
    fun testGetFieldData() {
        val res = bridge.getFieldData("Lorg/luckypray/dexkit/demo/MainActivity;->TAG:Ljava/lang/String;")
        assert(res != null)
        assert(res!!.className == "org.luckypray.dexkit.demo.MainActivity")
        assert(res.fieldName == "TAG")
        assert(res.typeName == "java.lang.String")
    }

    @Test
    fun testGetMethodUsingStrings() {
        val res = bridge.getMethodData("Lorg/luckypray/dexkit/demo/PlayActivity;->onCreate(Landroid/os/Bundle;)V")
        assert(res != null)
        val usingStrings = res!!.usingStrings
        assert(usingStrings.size == 2)
        usingStrings.containsAll(listOf("onCreate", "PlayActivity"))
    }

    @Test
    fun testMethodUsingNumbers() {
        val cls = bridge.findMethod {
            excludePackages("org.luckypray.dexkit.demo.hook")
            matcher {
                usingNumbers(0, -1, 0.01, 0.987, 114514)
            }
        }.single()
        assert(cls.className == "org.luckypray.dexkit.demo.PlayActivity")
    }

    @Test
    fun testConcurrentFindMethodOnSharedBridge() {
        DexKitBridge.create(demoApkPath).use { parallelBridge ->
            parallelBridge.setThreadNum(2)
            val workers = 4
            val iterationsPerWorker = 8
            val start = CountDownLatch(1)
            val executor = Executors.newFixedThreadPool(workers)
            try {
                val futures = (0 until workers).map {
                    executor.submit<Unit> {
                        start.await(10, TimeUnit.SECONDS)
                        repeat(iterationsPerWorker) {
                            val result = parallelBridge.findMethod {
                                excludePackages("org.luckypray.dexkit.demo.hook")
                                matcher {
                                    usingNumbers(114514)
                                }
                            }
                            assert(result.size == 2)
                        }
                    }
                }
                start.countDown()
                futures.forEach { it.get(60, TimeUnit.SECONDS) }
            } finally {
                executor.shutdownNow()
            }
        }
    }

    @Test
    fun testConcurrentFindFirstMethodOnSharedBridge() {
        DexKitBridge.create(demoApkPath).use { parallelBridge ->
            parallelBridge.setThreadNum(2)
            val workers = 4
            val iterationsPerWorker = 8
            val start = CountDownLatch(1)
            val executor = Executors.newFixedThreadPool(workers)
            try {
                val futures = (0 until workers).map {
                    executor.submit<Unit> {
                        start.await(10, TimeUnit.SECONDS)
                        repeat(iterationsPerWorker) {
                            val result = parallelBridge.findMethod {
                                findFirst = true
                                excludePackages("org.luckypray.dexkit.demo.hook")
                                matcher {
                                    usingNumbers(114514)
                                }
                            }
                            assert(result.size == 1)
                        }
                    }
                }
                start.countDown()
                futures.forEach { it.get(60, TimeUnit.SECONDS) }
            } finally {
                executor.shutdownNow()
            }
        }
    }

    @OptIn(DexKitExperimentalApi::class)
    @Test
    fun testConcurrentFindFirstMethodOnSharedBridgeWithSharedScheduler() {
        DexKitBridge.create(demoApkPath).use { parallelBridge ->
            parallelBridge.setThreadNum(2)
            parallelBridge.setSchedulerMode(SchedulerMode.SharedPool)
            parallelBridge.setMaxConcurrentQueries(2)
            val workers = 4
            val iterationsPerWorker = 8
            val start = CountDownLatch(1)
            val executor = Executors.newFixedThreadPool(workers)
            try {
                val futures = (0 until workers).map {
                    executor.submit<Unit> {
                        start.await(10, TimeUnit.SECONDS)
                        repeat(iterationsPerWorker) {
                            val result = parallelBridge.findMethod {
                                findFirst = true
                                excludePackages("org.luckypray.dexkit.demo.hook")
                                matcher {
                                    usingNumbers(114514)
                                }
                            }
                            assert(result.size == 1)
                        }
                    }
                }
                start.countDown()
                futures.forEach { it.get(60, TimeUnit.SECONDS) }
            } finally {
                executor.shutdownNow()
            }
        }
    }

    @OptIn(DexKitExperimentalApi::class)
    @Test
    fun testMixedFindFirstAndRegularQueriesOnSharedScheduler() {
        DexKitBridge.create(demoApkPath).use { parallelBridge ->
            parallelBridge.setThreadNum(2)
            parallelBridge.setSchedulerMode(SchedulerMode.SharedPool)
            parallelBridge.setMaxConcurrentQueries(2)
            val start = CountDownLatch(1)
            val executor = Executors.newFixedThreadPool(4)
            try {
                val futures = listOf(
                    executor.submit<Unit> {
                        start.await(10, TimeUnit.SECONDS)
                        repeat(8) {
                            val result = parallelBridge.findMethod {
                                excludePackages("org.luckypray.dexkit.demo.hook")
                                matcher {
                                    usingNumbers(114514)
                                }
                            }
                            assert(result.size == 2)
                        }
                    },
                    executor.submit<Unit> {
                        start.await(10, TimeUnit.SECONDS)
                        repeat(8) {
                            val result = parallelBridge.findMethod {
                                excludePackages("org.luckypray.dexkit.demo.hook")
                                matcher {
                                    usingNumbers(114514)
                                }
                            }
                            assert(result.size == 2)
                        }
                    },
                    executor.submit<Unit> {
                        start.await(10, TimeUnit.SECONDS)
                        repeat(8) {
                            val result = parallelBridge.findMethod {
                                findFirst = true
                                excludePackages("org.luckypray.dexkit.demo.hook")
                                matcher {
                                    usingNumbers(114514)
                                }
                            }
                            assert(result.size == 1)
                        }
                    },
                    executor.submit<Unit> {
                        start.await(10, TimeUnit.SECONDS)
                        repeat(8) {
                            val result = parallelBridge.findMethod {
                                findFirst = true
                                excludePackages("org.luckypray.dexkit.demo.hook")
                                matcher {
                                    usingNumbers(114514)
                                }
                            }
                            assert(result.size == 1)
                        }
                    }
                )
                start.countDown()
                futures.forEach { it.get(60, TimeUnit.SECONDS) }
            } finally {
                executor.shutdownNow()
            }
        }
    }

    @OptIn(DexKitExperimentalApi::class)
    @Test
    fun testSharedSchedulerMetricsSnapshot() {
        DexKitBridge.create(demoApkPath).use { parallelBridge ->
            parallelBridge.setThreadNum(2)
            parallelBridge.setSchedulerMode(SchedulerMode.SharedPool)
            parallelBridge.setMaxConcurrentQueries(2)
            val start = CountDownLatch(1)
            val executor = Executors.newFixedThreadPool(4)
            try {
                val futures = listOf(
                    executor.submit<Unit> {
                        start.await(10, TimeUnit.SECONDS)
                        repeat(8) {
                            val result = parallelBridge.findMethod {
                                excludePackages("org.luckypray.dexkit.demo.hook")
                                matcher {
                                    usingNumbers(114514)
                                }
                            }
                            assert(result.size == 2)
                        }
                    },
                    executor.submit<Unit> {
                        start.await(10, TimeUnit.SECONDS)
                        repeat(8) {
                            val result = parallelBridge.findMethod {
                                findFirst = true
                                excludePackages("org.luckypray.dexkit.demo.hook")
                                matcher {
                                    usingNumbers(114514)
                                }
                            }
                            assert(result.size == 1)
                        }
                    }
                )
                start.countDown()
                futures.forEach { it.get(60, TimeUnit.SECONDS) }
            } finally {
                executor.shutdownNow()
            }

            val metrics = parallelBridge.getSchedulerMetricsSnapshot()
            println(metrics)
            assert(metrics.dispatchedTasks > 0)
            assert(metrics.baseDispatchedTasks + metrics.bonusDispatchedTasks == metrics.dispatchedTasks)
            assert(metrics.baseDispatchedTasks > 0)
            assert(metrics.bonusDispatchedTasks > 0)
            assert(metrics.maxTotalInFlight > 0)
            assert(metrics.maxVisibleQueryShareCount >= 2)
            assert(metrics.shareCountSyncs > 0)
        }
    }

    @OptIn(DexKitExperimentalApi::class)
    @Test
    fun testSharedSchedulerMetricsReset() {
        DexKitBridge.create(demoApkPath).use { parallelBridge ->
            parallelBridge.setThreadNum(2)
            parallelBridge.setSchedulerMode(SchedulerMode.SharedPool)
            parallelBridge.setMaxConcurrentQueries(2)

            repeat(4) {
                val result = parallelBridge.findMethod {
                    excludePackages("org.luckypray.dexkit.demo.hook")
                    matcher {
                        usingNumbers(114514)
                    }
                }
                assert(result.size == 2)
            }

            val beforeReset = parallelBridge.getSchedulerMetricsSnapshot()
            assert(beforeReset.dispatchedTasks > 0)

            parallelBridge.resetSchedulerMetrics()

            val afterReset = parallelBridge.getSchedulerMetricsSnapshot()
            assert(afterReset.dispatchedTasks == 0L)
            assert(afterReset.baseDispatchedTasks == 0L)
            assert(afterReset.bonusDispatchedTasks == 0L)
            assert(afterReset.shareCountSyncs == 0L)
            assert(afterReset.maxTotalInFlight == 0L)
            assert(afterReset.maxVisibleQueryShareCount == 0L)
            assert(afterReset.maxRunnableQueueSize == 0L)
        }
    }

    @OptIn(DexKitExperimentalApi::class)
    @Test
    fun testLastQueryMetricsSnapshotOnSharedScheduler() {
        DexKitBridge.create(demoApkPath).use { parallelBridge ->
            parallelBridge.setThreadNum(2)
            parallelBridge.setSchedulerMode(SchedulerMode.SharedPool)
            parallelBridge.setMaxConcurrentQueries(2)
            parallelBridge.setQueryMetricsEnabled(true)

            val result = parallelBridge.findMethod {
                excludePackages("org.luckypray.dexkit.demo.hook")
                matcher {
                    usingNumbers(114514)
                }
            }
            assert(result.size == 2)

            val metrics = parallelBridge.getLastQueryMetricsSnapshot()
            println(metrics)
            assert(metrics.submittedTasks > 0)
            assert(metrics.dispatchedTasks > 0)
            assert(metrics.completedTasks == metrics.submittedTasks)
            assert(metrics.baseDispatchedTasks + metrics.bonusDispatchedTasks == metrics.dispatchedTasks)
            assert(metrics.maxInFlight > 0)
            assert(metrics.maxQueryShareCount > 0)
            assert(metrics.firstDispatchDelayNs >= 0)
            assert(metrics.firstTaskStartDelayNs >= 0)
            assert(metrics.lastTaskFinishDelayNs >= metrics.firstTaskStartDelayNs)
            assert(metrics.taskRuntimeTotalNs >= metrics.taskRuntimeMaxNs)
            assert(metrics.preprocessCompletedNs >= 0)
            assert(metrics.submissionCompletedNs >= metrics.preprocessCompletedNs)
            assert(metrics.workersCompletedNs >= metrics.submissionCompletedNs)
            assert(metrics.completedNs >= metrics.workersCompletedNs)
        }
    }

    @OptIn(DexKitExperimentalApi::class)
    @Test
    fun testLastQueryMetricsSnapshotIsThreadLocalOnSharedScheduler() {
        DexKitBridge.create(demoApkPath).use { parallelBridge ->
            parallelBridge.setThreadNum(2)
            parallelBridge.setSchedulerMode(SchedulerMode.SharedPool)
            parallelBridge.setMaxConcurrentQueries(2)
            parallelBridge.setQueryMetricsEnabled(true)
            val workers = 2
            val start = CountDownLatch(1)
            val executor = Executors.newFixedThreadPool(workers)
            try {
                val futures = (0 until workers).map {
                    executor.submit<Unit> {
                        start.await(10, TimeUnit.SECONDS)
                        repeat(4) {
                            val result = parallelBridge.findMethod {
                                excludePackages("org.luckypray.dexkit.demo.hook")
                                matcher {
                                    usingNumbers(114514)
                                }
                            }
                            assert(result.size == 2)
                            val metrics = parallelBridge.getLastQueryMetricsSnapshot()
                            assert(metrics.submittedTasks > 0)
                            assert(metrics.dispatchedTasks > 0)
                            assert(metrics.completedTasks == metrics.submittedTasks)
                            assert(metrics.baseDispatchedTasks + metrics.bonusDispatchedTasks == metrics.dispatchedTasks)
                            assert(metrics.maxInFlight > 0)
                            assert(metrics.firstTaskStartDelayNs >= 0)
                            assert(metrics.lastTaskFinishDelayNs >= metrics.firstTaskStartDelayNs)
                            assert(metrics.taskRuntimeTotalNs >= metrics.taskRuntimeMaxNs)
                            assert(metrics.completedNs >= metrics.workersCompletedNs)
                        }
                    }
                }
                start.countDown()
                futures.forEach { it.get(60, TimeUnit.SECONDS) }
            } finally {
                executor.shutdownNow()
            }
        }
    }

    @OptIn(DexKitExperimentalApi::class)
    @Test
    fun testQueryMetricsHistorySnapshotOnSharedScheduler() {
        DexKitBridge.create(demoApkPath).use { parallelBridge ->
            parallelBridge.setThreadNum(2)
            parallelBridge.setSchedulerMode(SchedulerMode.SharedPool)
            parallelBridge.setMaxConcurrentQueries(2)
            parallelBridge.setQueryMetricsEnabled(true)

            val regularResult = parallelBridge.findMethod {
                excludePackages("org.luckypray.dexkit.demo.hook")
                matcher {
                    usingNumbers(114514)
                }
            }
            assert(regularResult.size == 2)

            val firstResult = parallelBridge.findMethod {
                findFirst = true
                excludePackages("org.luckypray.dexkit.demo.hook")
                matcher {
                    usingNumbers(114514)
                }
            }
            assert(firstResult.size == 1)

            val history = parallelBridge.getQueryMetricsHistorySnapshot()
            println(history)
            assert(history.droppedRecords == 0L)
            assert(history.records.size == 2)

            val firstRecord = history.records[0]
            assert(firstRecord.kind == QueryMetricsKind.FIND_METHOD)
            assert(firstRecord.priority == QueryMetricsPriority.NORMAL)

            val secondRecord = history.records[1]
            assert(secondRecord.kind == QueryMetricsKind.FIND_METHOD)
            assert(secondRecord.priority == QueryMetricsPriority.LATENCY_SENSITIVE)

            history.records.forEach { record ->
                assert(record.metrics.submittedTasks > 0)
                assert(record.metrics.dispatchedTasks > 0)
                assert(record.metrics.completedTasks <= record.metrics.submittedTasks)
                assert(record.metrics.dispatchedTasks <= record.metrics.submittedTasks)
                assert(record.metrics.baseDispatchedTasks + record.metrics.bonusDispatchedTasks == record.metrics.dispatchedTasks)
                assert(record.metrics.preprocessCompletedNs >= 0)
                assert(record.metrics.submissionCompletedNs >= record.metrics.preprocessCompletedNs)
                assert(record.metrics.workersCompletedNs >= record.metrics.submissionCompletedNs)
                assert(record.metrics.completedNs >= record.metrics.workersCompletedNs)
            }
        }
    }

    @OptIn(DexKitExperimentalApi::class)
    @Test
    fun testConcurrentMixedQueryHistoryOnSharedScheduler() {
        DexKitBridge.create(demoApkPath).use { parallelBridge ->
            parallelBridge.setThreadNum(2)
            parallelBridge.setSchedulerMode(SchedulerMode.SharedPool)
            parallelBridge.setMaxConcurrentQueries(2)
            parallelBridge.setQueryMetricsEnabled(true)
            parallelBridge.resetQueryMetricsHistory()

            val start = CountDownLatch(1)
            val executor = Executors.newFixedThreadPool(2)
            try {
                val futures = listOf(
                    executor.submit<Unit> {
                        start.await(10, TimeUnit.SECONDS)
                        val result = parallelBridge.findMethod {
                            excludePackages("org.luckypray.dexkit.demo.hook")
                            matcher {
                                usingNumbers(114514)
                            }
                        }
                        assert(result.size == 2)
                    },
                    executor.submit<Unit> {
                        start.await(10, TimeUnit.SECONDS)
                        val result = parallelBridge.findMethod {
                            findFirst = true
                            excludePackages("org.luckypray.dexkit.demo.hook")
                            matcher {
                                usingNumbers(114514)
                            }
                        }
                        assert(result.size == 1)
                    }
                )
                start.countDown()
                futures.forEach { it.get(60, TimeUnit.SECONDS) }
            } finally {
                executor.shutdownNow()
            }

            val history = parallelBridge.getQueryMetricsHistorySnapshot()
            println(history)
            assert(history.records.size == 2)

            val recordsByPriority = history.records.associateBy { it.priority }
            val normal = recordsByPriority[QueryMetricsPriority.NORMAL]
                ?: error("Missing normal query metrics record")
            val latencySensitive = recordsByPriority[QueryMetricsPriority.LATENCY_SENSITIVE]
                ?: error("Missing latency-sensitive query metrics record")

            assert(normal.metrics.baseDispatchedTasks > 0)
            assert(normal.metrics.bonusDispatchedTasks == 0L)
            assert(latencySensitive.metrics.baseDispatchedTasks > 0)
            assert(latencySensitive.metrics.bonusDispatchedTasks > 0)
        }
    }

    @OptIn(DexKitExperimentalApi::class)
    @Test
    fun testThreeWayConcurrentQueryHistoryOnSharedScheduler() {
        DexKitBridge.create(demoApkPath).use { parallelBridge ->
            parallelBridge.setThreadNum(2)
            parallelBridge.setSchedulerMode(SchedulerMode.SharedPool)
            parallelBridge.setMaxConcurrentQueries(3)
            parallelBridge.setQueryMetricsEnabled(true)
            parallelBridge.resetQueryMetricsHistory()

            val start = CountDownLatch(1)
            val executor = Executors.newFixedThreadPool(3)
            try {
                val futures = listOf(
                    executor.submit<Unit> {
                        start.await(10, TimeUnit.SECONDS)
                        val result = parallelBridge.findMethod {
                            excludePackages("org.luckypray.dexkit.demo.hook")
                            matcher {
                                usingNumbers(114514)
                            }
                        }
                        assert(result.size == 2)
                    },
                    executor.submit<Unit> {
                        start.await(10, TimeUnit.SECONDS)
                        val result = parallelBridge.findMethod {
                            excludePackages("org.luckypray.dexkit.demo.hook")
                            matcher {
                                usingNumbers(114514)
                            }
                        }
                        assert(result.size == 2)
                    },
                    executor.submit<Unit> {
                        start.await(10, TimeUnit.SECONDS)
                        val result = parallelBridge.findMethod {
                            findFirst = true
                            excludePackages("org.luckypray.dexkit.demo.hook")
                            matcher {
                                usingNumbers(114514)
                            }
                        }
                        assert(result.size == 1)
                    }
                )
                start.countDown()
                futures.forEach { it.get(60, TimeUnit.SECONDS) }
            } finally {
                executor.shutdownNow()
            }

            val history = parallelBridge.getQueryMetricsHistorySnapshot()
            println(history)
            assert(history.records.size == 3)

            val normalRecords = history.records.filter { it.priority == QueryMetricsPriority.NORMAL }
            val latencySensitiveRecords = history.records.filter { it.priority == QueryMetricsPriority.LATENCY_SENSITIVE }

            assert(normalRecords.size == 2)
            assert(latencySensitiveRecords.size == 1)
            normalRecords.forEach { record ->
                assert(record.metrics.baseDispatchedTasks > 0)
                assert(record.metrics.bonusDispatchedTasks == 0L)
            }
            val latencySensitive = latencySensitiveRecords.single()
            assert(latencySensitive.metrics.baseDispatchedTasks > 0)
            assert(latencySensitive.metrics.bonusDispatchedTasks > 0)
        }
    }

    @OptIn(DexKitExperimentalApi::class)
    @Test
    fun testQueryMetricsHistoryResetOnSharedScheduler() {
        DexKitBridge.create(demoApkPath).use { parallelBridge ->
            parallelBridge.setThreadNum(2)
            parallelBridge.setSchedulerMode(SchedulerMode.SharedPool)
            parallelBridge.setMaxConcurrentQueries(2)
            parallelBridge.setQueryMetricsEnabled(true)

            val result = parallelBridge.findMethod {
                excludePackages("org.luckypray.dexkit.demo.hook")
                matcher {
                    usingNumbers(114514)
                }
            }
            assert(result.size == 2)

            val beforeReset = parallelBridge.getQueryMetricsHistorySnapshot()
            assert(beforeReset.droppedRecords == 0L)
            assert(beforeReset.records.size == 1)

            parallelBridge.resetQueryMetricsHistory()

            val afterReset = parallelBridge.getQueryMetricsHistorySnapshot()
            assert(afterReset.droppedRecords == 0L)
            assert(afterReset.records.isEmpty())
        }
    }

    @Test
    fun testConcurrentBatchFindClassUsingStringsOnSharedBridge() {
        DexKitBridge.create(demoApkPath).use { parallelBridge ->
            parallelBridge.setThreadNum(2)
            val workers = 4
            val iterationsPerWorker = 8
            val start = CountDownLatch(1)
            val executor = Executors.newFixedThreadPool(workers)
            try {
                val futures = (0 until workers).map {
                    executor.submit<Unit> {
                        start.await(10, TimeUnit.SECONDS)
                        repeat(iterationsPerWorker) {
                            val result = parallelBridge.batchFindClassUsingStrings {
                                searchPackages("org.luckypray.dexkit.demo")
                                groups(
                                    mapOf(
                                        "main_activity" to listOf("onClick: playButton"),
                                        "play_activity" to listOf("onClick: rollButton")
                                    )
                                )
                            }
                            assert(result["main_activity"]?.size == 1)
                            assert(result["play_activity"]?.size == 1)
                        }
                    }
                }
                start.countDown()
                futures.forEach { it.get(60, TimeUnit.SECONDS) }
            } finally {
                executor.shutdownNow()
            }
        }
    }

    @Test
    fun testConcurrentBatchFindMethodUsingStringsOnSharedBridge() {
        DexKitBridge.create(demoApkPath).use { parallelBridge ->
            parallelBridge.setThreadNum(2)
            val workers = 4
            val iterationsPerWorker = 8
            val start = CountDownLatch(1)
            val executor = Executors.newFixedThreadPool(workers)
            try {
                val futures = (0 until workers).map {
                    executor.submit<Unit> {
                        start.await(10, TimeUnit.SECONDS)
                        repeat(iterationsPerWorker) {
                            val result = parallelBridge.batchFindMethodUsingStrings {
                                searchPackages("org.luckypray.dexkit.demo")
                                groups(
                                    mapOf(
                                        "main_on_click" to listOf("onClick: playButton"),
                                        "play_on_click" to listOf("onClick: rollButton")
                                    )
                                )
                            }
                            assert(result["main_on_click"]?.size == 1)
                            assert(result["play_on_click"]?.size == 1)
                        }
                    }
                }
                start.countDown()
                futures.forEach { it.get(60, TimeUnit.SECONDS) }
            } finally {
                executor.shutdownNow()
            }
        }
    }

    @OptIn(DexKitExperimentalApi::class)
    @Test
    fun testConcurrentBatchFindMethodUsingStringsOnSharedScheduler() {
        DexKitBridge.create(demoApkPath).use { parallelBridge ->
            parallelBridge.setThreadNum(2)
            parallelBridge.setSchedulerMode(SchedulerMode.SharedPool)
            parallelBridge.setMaxConcurrentQueries(2)
            val workers = 4
            val iterationsPerWorker = 8
            val start = CountDownLatch(1)
            val executor = Executors.newFixedThreadPool(workers)
            try {
                val futures = (0 until workers).map {
                    executor.submit<Unit> {
                        start.await(10, TimeUnit.SECONDS)
                        repeat(iterationsPerWorker) {
                            val result = parallelBridge.batchFindMethodUsingStrings {
                                searchPackages("org.luckypray.dexkit.demo")
                                groups(
                                    mapOf(
                                        "main_on_click" to listOf("onClick: playButton"),
                                        "play_on_click" to listOf("onClick: rollButton")
                                    )
                                )
                            }
                            assert(result["main_on_click"]?.size == 1)
                            assert(result["play_on_click"]?.size == 1)
                        }
                    }
                }
                start.countDown()
                futures.forEach { it.get(60, TimeUnit.SECONDS) }
            } finally {
                executor.shutdownNow()
            }
        }
    }

    @Test
    fun testConcurrentNativeGetMethodUsingStringsWithoutBridgeSynchronization() {
        DexKitBridge.create(demoApkPath).use { parallelBridge ->
            val method = parallelBridge.getMethodData("Lorg/luckypray/dexkit/demo/PlayActivity;->onCreate(Landroid/os/Bundle;)V")
            assert(method != null)
            val encodeId = method!!.getEncodeId()
            val token = getBridgeToken(parallelBridge)
            val workers = 6
            val iterationsPerWorker = 16
            val start = CountDownLatch(1)
            val baseline = AtomicReference<List<String>?>(null)
            val executor = Executors.newFixedThreadPool(workers)
            try {
                val futures = (0 until workers).map {
                    executor.submit<Unit> {
                        start.await(10, TimeUnit.SECONDS)
                        repeat(iterationsPerWorker) {
                            val result = nativeGetMethodUsingStrings(token, encodeId)
                            assert(result.size == 2)
                            assert(result.containsAll(listOf("onCreate", "PlayActivity")))
                            val current = baseline.get()
                            if (current == null) {
                                baseline.compareAndSet(null, result)
                            } else {
                                assert(result == current)
                            }
                        }
                    }
                }
                start.countDown()
                futures.forEach { it.get(60, TimeUnit.SECONDS) }
            } finally {
                executor.shutdownNow()
            }
        }
    }

    @Test
    fun testConcurrentNativeGetMethodOpCodesWithoutBridgeSynchronization() {
        DexKitBridge.create(demoApkPath).use { parallelBridge ->
            val method = parallelBridge.getMethodData("Lorg/luckypray/dexkit/demo/MainActivity;->onClick(Landroid/view/View;)V")
            assert(method != null)
            val encodeId = method!!.getEncodeId()
            val token = getBridgeToken(parallelBridge)
            val workers = 6
            val iterationsPerWorker = 16
            val start = CountDownLatch(1)
            val baseline = AtomicReference<List<Int>?>(null)
            val executor = Executors.newFixedThreadPool(workers)
            try {
                val futures = (0 until workers).map {
                    executor.submit<Unit> {
                        start.await(10, TimeUnit.SECONDS)
                        repeat(iterationsPerWorker) {
                            val result = nativeGetMethodOpCodes(token, encodeId)
                            assert(result.isNotEmpty())
                            val current = baseline.get()
                            if (current == null) {
                                baseline.compareAndSet(null, result)
                            } else {
                                assert(result == current)
                            }
                        }
                    }
                }
                start.countDown()
                futures.forEach { it.get(60, TimeUnit.SECONDS) }
            } finally {
                executor.shutdownNow()
            }
        }
    }
}
