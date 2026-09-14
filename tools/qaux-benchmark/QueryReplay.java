/*
 * Local query adapters derived from QAuxiliary (cinit/QAuxiliary), commit
 * 01801ffd013c95781dd360704adf48dc42ee8aa6. Original query logic:
 * Copyright (C) 2019-2022 qwq233@qwq2333.top and QAuxiliary contributors.
 * QAuxiliary source notices specify AGPL-3.0-or-later and its EULA:
 * https://github.com/cinit/QAuxiliary/blob/01801ffd013c95781dd360704adf48dc42ee8aa6/LICENSE.md
 * This local benchmark adapter does not change the license of DexKit.
 */

import org.luckypray.dexkit.DexKitBridge;
import org.luckypray.dexkit.query.BatchFindClassUsingStrings;
import org.luckypray.dexkit.query.BatchFindMethodUsingStrings;
import org.luckypray.dexkit.query.FindClass;
import org.luckypray.dexkit.query.FindMethod;
import org.luckypray.dexkit.query.enums.StringMatchType;
import org.luckypray.dexkit.query.matchers.ClassMatcher;
import org.luckypray.dexkit.query.matchers.FieldMatcher;
import org.luckypray.dexkit.query.matchers.MethodMatcher;
import org.luckypray.dexkit.result.ClassData;
import org.luckypray.dexkit.result.ClassDataList;
import org.luckypray.dexkit.result.MethodData;
import org.luckypray.dexkit.result.MethodDataList;

import java.lang.reflect.Modifier;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Base64;
import java.util.Collection;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.TreeSet;
import java.util.function.Supplier;

public final class QueryReplay {
    private static final List<Object> stages = new ArrayList<>();
    private static final List<Object> features = new ArrayList<>();
    private static int pass;
    private static String feature;
    private static int apiErrors;
    private static boolean measure;
    private static native long[] memorySnapshot();

    private static Map<String, Object> memory(long[] values) {
        return obj("rss_bytes", values[0], "process_peak_rss_bytes", values[1],
                "process_malloc_in_use_bytes", values[2], "process_malloc_reserved_bytes", values[3],
                "process_malloc_blocks", values[4], "physical_footprint_bytes", values[5],
                "process_peak_footprint_bytes", values[6]);
    }

    private static Map<String, Object> obj(Object... values) {
        Map<String, Object> result = new LinkedHashMap<>();
        for (int i = 0; i < values.length; i += 2) result.put((String) values[i], values[i + 1]);
        return result;
    }

    private static String json(Object value) {
        if (value == null) return "null";
        if (value instanceof Number || value instanceof Boolean) return value.toString();
        if (value instanceof Map) {
            List<String> parts = new ArrayList<>();
            ((Map<?, ?>) value).forEach((k, v) -> parts.add(json(k.toString()) + ":" + json(v)));
            return "{" + String.join(",", parts) + "}";
        }
        if (value instanceof Collection) {
            List<String> parts = new ArrayList<>();
            for (Object item : (Collection<?>) value) parts.add(json(item));
            return "[" + String.join(",", parts) + "]";
        }
        StringBuilder out = new StringBuilder("\"");
        for (char c : value.toString().toCharArray()) {
            if (c == '"' || c == '\\') out.append('\\').append(c);
            else if (c < 32) out.append(String.format("\\u%04x", (int) c));
            else out.append(c);
        }
        return out.append('"').toString();
    }

    private static String digest(List<String> values) {
        try {
            List<String> sorted = new ArrayList<>(values);
            Collections.sort(sorted); // Retain duplicates; result multiplicity matters.
            byte[] bytes = MessageDigest.getInstance("SHA-256").digest(
                    String.join("\n", sorted).getBytes(StandardCharsets.UTF_8));
            StringBuilder out = new StringBuilder();
            for (byte b : bytes) out.append(String.format("%02x", b & 255));
            return out.toString();
        } catch (Exception e) { throw new IllegalStateException(e); }
    }

    private static List<String> descriptors(Collection<?> values) {
        List<String> result = new ArrayList<>();
        for (Object value : values) {
            result.add(value instanceof MethodData ? ((MethodData) value).getDescriptor()
                    : ((ClassData) value).getName());
        }
        return result;
    }

    private static <T extends Collection<?>> T stage(String name, Supplier<T> query) {
        long begin = System.nanoTime();
        T result = query.get();
        long returned = System.nanoTime();
        List<String> values = measure ? null : descriptors(result);
        long materialized = System.nanoTime();
        Map<String, Object> record = obj("pass", pass, "feature", feature, "stage", name,
                "api_ns", returned - begin, "descriptor_ns", materialized - returned,
                "count", result.size());
        if (!measure) {
            record.put("ordered_results", values);
            record.put("multiset_sha256", digest(values));
        }
        stages.add(record);
        if (!measure) System.err.printf("pass=%d %s/%s count=%d api=%.3f ms%n",
                pass, feature, name, result.size(), (returned - begin) / 1e6);
        return result;
    }

    private static final class NoMatch extends RuntimeException {
        NoMatch(String message) { super(message); }
    }

    private static <T> T single(List<T> results, String step) {
        if (results.size() != 1) throw new NoMatch(step + ": expected one, got " + results.size());
        return results.get(0);
    }

    private static <T> T first(List<T> results, String step) {
        if (results.isEmpty()) throw new NoMatch(step + ": empty");
        // Preserve QAuxiliary's firstOrNull after a complete findMethod query.
        // Do not turn it into native findFirst, which changes the workload.
        return results.get(0);
    }

    private static void scenario(String name, String scope, Supplier<Collection<MethodData>> body) {
        feature = name;
        long start = System.nanoTime();
        Map<String, Object> record = obj("pass", pass, "feature", name, "scope", scope);
        try {
            record.put("selected", descriptors(body.get()));
            record.put("status", "resolved");
        } catch (NoMatch e) {
            record.put("status", "unresolved");
            record.put("reason", e.getMessage());
        } catch (Exception | LinkageError e) {
            apiErrors++;
            record.put("status", "error");
            record.put("reason", e.toString());
        }
        record.put("observed_ns", System.nanoTime() - start);
        features.add(record);
        if (!measure) System.err.println(json(record));
    }

    private static MethodDataList methods(DexKitBridge b, MethodMatcher matcher) {
        return b.findMethod(FindMethod.create().matcher(matcher));
    }

    private static MethodDataList methods(DexKitBridge b, String pkg, MethodMatcher matcher) {
        return b.findMethod(FindMethod.create().searchPackages(pkg).matcher(matcher));
    }

    private static Map<String, Collection<String>> loadGroups(Path path) throws Exception {
        Map<String, Collection<String>> groups = new LinkedHashMap<>();
        for (String line : Files.readAllLines(path)) {
            String[] fields = line.split("\t", -1);
            List<String> strings = new ArrayList<>();
            for (int i = 1; i < fields.length; i++) {
                strings.add(new String(Base64.getDecoder().decode(fields[i]), StandardCharsets.UTF_8));
            }
            groups.put(fields[0], strings);
        }
        if (groups.isEmpty()) throw new IllegalArgumentException("empty group corpus");
        return groups;
    }

    private static void batch(DexKitBridge bridge, Map<String, Collection<String>> groups) {
        long begin = System.nanoTime();
        Map<String, MethodDataList> found = bridge.batchFindMethodUsingStrings(
                BatchFindMethodUsingStrings.create().groups(groups, StringMatchType.SimilarRegex));
        long returned = System.nanoTime();
        if (!groups.keySet().containsAll(found.keySet())) throw new IllegalStateException("Unexpected batch result key");
        Map<String, Object> results = new LinkedHashMap<>();
        Map<String, TreeSet<String>> targetUnion = new LinkedHashMap<>();
        for (String key : groups.keySet()) {
            MethodDataList methods = found.get(key);
            if (measure) {
                results.put(key, obj("count", methods == null ? 0 : methods.size()));
            } else {
                List<String> values = descriptors(methods == null ? Collections.emptyList() : methods);
                results.put(key, obj("count", values.size(), "ordered_results", values,
                        "multiset_sha256", digest(values)));
                targetUnion.computeIfAbsent(key.split("#_#")[0], ignored -> new TreeSet<>()).addAll(values);
            }
        }
        long hitTargets = targetUnion.values().stream().filter(v -> !v.isEmpty()).count();
        Map<String, Object> record = obj("pass", pass, "feature", "all_literal_targets", "stage", "batch_strings",
                "api_ns", returned - begin, "postprocess_ns", System.nanoTime() - returned,
                "group_count", groups.size(), "host_filters_applied", false, "groups", results,
                "returned_keys", new TreeSet<>(found.keySet()));
        if (!measure) {
            record.put("target_count", targetUnion.size());
            record.put("targets_with_raw_candidates", hitTargets);
            record.put("target_candidate_union", targetUnion);
        }
        stages.add(record);
        if (!measure) System.err.printf("pass=%d batch groups=%d raw-hit-targets=%d/%d api=%.3f ms%n",
                pass, groups.size(), hitTargets, targetUnion.size(), (returned - begin) / 1e6);
    }

    private static void chains(DexKitBridge b) {
        scenario("AIOMsgItem_initContentDescription", "exact finder query chain", () -> {
            MethodData seed = single(stage("find_seed", () -> methods(b, MethodMatcher.create()
                    .declaredClass("com.tencent.mobileqq.aio.msg.AIOMsgItem")
                    .usingStrings("senderUid", "peerUid"))), "find_seed");
            MethodData result = single(stage("find_callee_using_seed_as_caller", () -> methods(b,
                    MethodMatcher.create().declaredClass("com.tencent.mobileqq.aio.msg.AIOMsgItem")
                            .returnType("java.lang.String").addCaller(seed.getDescriptor()))), "find_callee");
            return List.of(result);
        });

        scenario("EmotionDetailAi", "exact finder query chain", () -> {
            ClassDataList classes = stage("find_classes", () -> b.findClass(FindClass.create()
                    .searchPackages("com.tencent.mobileqq.emotionintegrate")
                    .matcher(ClassMatcher.create().usingStrings("MsgEmoticonPreviewData", "doRestoreSaveInstanceState"))));
            MethodData result = single(stage("find_method_in_classes", () -> classes.findMethod(
                    FindMethod.create().matcher(MethodMatcher.create().returnType("boolean").usingNumbers(14)))), "method");
            return List.of(result);
        });

        scenario("Hd_HideEmoReplyLayout_Method", "exact finder; Android type represented by its name", () -> {
            ClassDataList classes = stage("find_classes", () -> b.findClass(FindClass.create()
                    .searchPackages("com.tencent.mobileqq.aio.msglist.holder.template")
                    .matcher(ClassMatcher.create().usingStrings("AIOReceiverBubbleTemplate", "msgTailContainer"))));
            MethodData result = single(stage("find_method_in_classes", () -> classes.findMethod(
                    FindMethod.create().matcher(MethodMatcher.create().returnType("android.view.View")
                            .usingStrings("msgTailContainer")))), "method");
            return List.of(result);
        });

        scenario("BlockPicByMd5_LoadImagePathV2", "exact finder query", () -> List.of(single(
                stage("find_method", () -> methods(b, "com.tencent.mobileqq.aio.msglist.holder",
                        MethodMatcher.create().returnType("void")
                                .paramTypes("android.widget.ImageView", "java.lang.String", null, "int", "int",
                                        "com.tencent.qqnt.kernel.nativeinterface.MsgElement",
                                        "com.tencent.mobileqq.aio.msg.AIOMsgItem", null, "kotlin.jvm.functions.Function2")
                                .usingStrings("picView", "imagePath", "msgElement", "msgItem", "loadingImage"))), "method")));

        scenario("BlockPicByMd5_PicPathResolverV2", "exact finder with nested invoke matcher", () -> List.of(single(
                stage("find_method", () -> methods(b, "com.tencent.mobileqq.aio", MethodMatcher.create()
                        .returnType("java.lang.String").paramTypes("com.tencent.qqnt.kernel.nativeinterface.PicElement")
                        .addInvoke(MethodMatcher.create().name("assembleMobileQQRichMediaFilePath")
                                .returnType("java.lang.String")
                                .paramTypes("com.tencent.qqnt.kernel.nativeinterface.RichMediaFilePathInfo")))), "method")));

        scenario("ReplyNoAtHook", "exact ordered fallback queries; optional has-info lookup", () -> {
            MethodDataList candidates = stage("reply_new_signature", () -> methods(b,
                    "com.tencent.mobileqq.aio.input", MethodMatcher.create()
                            .paramTypes("com.tencent.mvi.base.route.MsgIntent").usingStrings("mContext", "senderUid")));
            if (candidates.isEmpty()) candidates = stage("reply_middle_signature", () -> methods(b,
                    "com.tencent.mobileqq.aio.input", MethodMatcher.create()
                            .paramTypes("com.tencent.mobileqq.aio.msg.AIOMsgItem").usingStrings("mContext", "senderUid")));
            if (candidates.isEmpty()) candidates = stage("reply_old_signature", () -> methods(b,
                    "com.tencent.mobileqq.aio.input", MethodMatcher.create()
                            .paramTypes("com.tencent.mobileqq.aio.msg.AIOMsgItem").usingStrings("msgItem.msgRecord.senderUid")));
            List<MethodData> selected = new ArrayList<>();
            selected.add(first(candidates, "reply"));
            MethodDataList optional = stage("optional_has_info", () -> methods(b,
                    "com.tencent.mobileqq.aio.utils", MethodMatcher.create().returnType("boolean")
                            .paramTypes("com.tencent.mobileqq.aio.msg.AIOMsgItem")
                            .addInvoke(MethodMatcher.create().name("getMsgRecord"))
                            .addUsingField(FieldMatcher.create().name("anonymousExtInfo"))));
            if (!optional.isEmpty()) selected.add(optional.get(0));
            return selected;
        });

        scenario("MultiForwardAvatarHook", "exact finder query; hook installation excluded", () -> List.of(single(
                stage("find_avatar_listener", () -> methods(b, MethodMatcher.create()
                        .declaredClass("com.tencent.mobileqq.aio.msglist.holder.component.avatar.AIOAvatarContentComponent")
                        .returnType("void").paramTypes()
                        .addInvoke(MethodMatcher.create().name("setOnClickListener")))), "method")));

        scenario("SettingEntryHook", "exact discovery query only; later reflection excluded", () -> List.of(single(
                stage("find_processor", () -> methods(b, MethodMatcher.create().addEqString("SimpleItemProcessor"))), "method")));

        scenario("HideMiniAppPullEntry", "QQ 9.3.55 query fallback; direct Conversation class resolved from DEX instead of the host ClassLoader", () -> {
            List<ClassData> classes = stage("resolve_direct_conversation_class", () -> {
                for (String name : List.of("com.tencent.mobileqq.activity.home.Conversation",
                        "com.tencent.mobileqq.activity.Conversation")) {
                    ClassData value = b.getClassData(name);
                    if (value != null) return List.of(value);
                }
                return Collections.emptyList();
            });
            String className = first(classes, "conversation_class").getName();
            List<MethodData> legacy = stage("legacy_string_groups", () -> {
                Map<String, Collection<String>> groups = new LinkedHashMap<>();
                groups.put("Conversation_0", List.of("initMiniAppEntryLayout."));
                groups.put("Conversation_1", List.of("initMicroAppEntryLayout."));
                groups.put("Conversation_2", List.of("init Mini App, cost="));
                Map<String, MethodDataList> result = b.batchFindMethodUsingStrings(
                        BatchFindMethodUsingStrings.create().groups(groups));
                List<MethodData> values = new ArrayList<>();
                result.values().forEach(values::addAll);
                return values;
            });
            for (MethodData method : legacy) {
                if (method.getClassName().equals(className) && method.getMethodSign().equals("()V")) {
                    return List.of(method);
                }
            }
            MethodDataList nt = stage("nt_invoke_fallback", () -> methods(b, MethodMatcher.create()
                    .addInvoke(MethodMatcher.create().declaredClass("com.tencent.mobileqq.mini.api.IMiniAppService")
                            .name("createMiniAppEntryManager"))));
            for (MethodData method : nt) {
                if (method.getClassName().equals(className) && method.getMethodSign().equals("()V")) {
                    return List.of(method);
                }
            }
            throw new NoMatch("no candidate passed the Conversation/()V verification");
        });

        scenario("AutoReceiveOriginalPhoto_cache_miss", "QQ 9.3.55 doFind query path with absent CAIOPictureView cache; class loading excluded", () -> {
            List<MethodData> selected = new ArrayList<>();
            selected.add(first(stage("nt_on_init_view", () -> methods(b, MethodMatcher.create()
                    .name("onInitView").usingStrings("rootView", "em_bas_view_the_original_picture"))), "nt_on_init_view"));
            List<ClassData> classes = stage("class_cache_miss_batch", () -> {
                Map<String, Collection<String>> groups = new LinkedHashMap<>();
                groups.put("1", List.of("AIOPictureView", "0X800A91E"));
                groups.put("2", List.of("AIOGalleryPicView", "0X800A91E"));
                Map<String, ClassDataList> result = b.batchFindClassUsingStrings(BatchFindClassUsingStrings.create().groups(groups));
                List<ClassData> all = new ArrayList<>();
                result.values().forEach(all::addAll); // Source does not deduplicate across groups.
                return all;
            });
            String className = single(classes, "picture_class").getName();
            selected.add(first(stage("download_original_click", () -> methods(b, MethodMatcher.create()
                    .modifiers(Modifier.PUBLIC).declaredClass(className).returnType("void").paramTypes()
                    .addCaller(MethodMatcher.create().declaredClass(className).name("onClick"))
                    .addInvoke(MethodMatcher.create().returnType("void").paramTypes("long", "int", "int")))), "on_click"));
            selected.add(single(stage("set_visibility", () -> methods(b, MethodMatcher.create()
                    .declaredClass(className).returnType("void").paramTypes("boolean")
                    .addInvoke("Landroid/widget/TextView;->setVisibility(I)V"))), "set_visibility"));
            return selected;
        });
    }

    private static void diagnostics(DexKitBridge b) {
        feature = "diagnostics_only";
        stage("load_image_strings_without_signature", () -> methods(b,
                "com.tencent.mobileqq.aio.msglist.holder", MethodMatcher.create()
                        .usingStrings("picView", "imagePath", "msgElement", "msgItem", "loadingImage")));
        stage("avatar_declared_class_exists", () -> {
            ClassData value = b.getClassData("com.tencent.mobileqq.aio.msglist.holder.component.avatar.AIOAvatarContentComponent");
            return value == null ? Collections.emptyList() : List.of(value);
        });
        stage("original_photo_strings_without_method_name", () -> methods(b, MethodMatcher.create()
                .usingStrings("rootView", "em_bas_view_the_original_picture")));
        stage("original_photo_resource_string_only", () -> methods(b, MethodMatcher.create()
                .usingStrings("em_bas_view_the_original_picture")));
    }

    public static void main(String[] args) throws Exception {
        if (args.length < 3 || args.length > 7) throw new IllegalArgumentException(
                "Usage: QueryReplay apk groups.tsv report.json [threads=4] [passes=2] [all|chains|batch|diagnostics] [verify|measure]");
        int threads = args.length > 3 ? Integer.parseInt(args[3]) : 4;
        int passes = args.length > 4 ? Integer.parseInt(args[4]) : 2;
        String profile = args.length > 5 ? args[5] : "all";
        String mode = args.length > 6 ? args[6] : "verify";
        if (!List.of("verify", "measure").contains(mode)) throw new IllegalArgumentException("invalid mode");
        measure = mode.equals("measure");
        if (threads < 1 || passes < 1 || !List.of("all", "chains", "batch", "diagnostics").contains(profile)) {
            throw new IllegalArgumentException("invalid threads, passes, or profile");
        }
        Map<String, Collection<String>> groups = loadGroups(Path.of(args[1]));
        Map<String, Object> report = obj("apk", Path.of(args[0]).toAbsolutePath().toString(),
                "qaux_commit", "01801ffd013c95781dd360704adf48dc42ee8aa6", "threads", threads,
                "passes", passes, "profile", profile, "mode", mode, "os", System.getProperty("os.name"),
                "arch", System.getProperty("os.arch"), "java", System.getProperty("java.runtime.version"),
                "scope", "Compatibility replay, not an Android end-to-end benchmark. No host reflection, hooks, or persistent descriptor cache. Fixed scenario order; all literal targets are a superset.",
                "measurement_note", "API times include query construction/JNI/result return. Measure mode keeps counts, required control-flow metadata and selected descriptors, but omits full-result hashing, retained raw result reports and per-stage logging. Verify mode includes those diagnostics. Later stages share caches warmed by preceding stages. Pass 2 repeats engine work without a QAux persistent cache. Library loading is recorded separately; create-to-close includes thread configuration.");
        long beforeLoad = System.nanoTime();
        System.loadLibrary("dexkit");
        report.put("load_library_ns", System.nanoTime() - beforeLoad);
        String probePath = System.getProperty("qaux.memory.probe");
        if (probePath != null) System.load(probePath);
        long[] memoryBefore = probePath == null ? null : memorySnapshot();
        long start = System.nanoTime();
        DexKitBridge bridge = DexKitBridge.create(args[0]);
        report.put("create_ns", System.nanoTime() - start);
        try {
            report.put("dex_num", bridge.getDexNum());
            long configure = System.nanoTime();
            bridge.setThreadNum(threads);
            report.put("configure_threads_ns", System.nanoTime() - configure);
            for (pass = 0; pass < passes; pass++) {
                if (profile.equals("all") || profile.equals("batch")) batch(bridge, groups);
                if (profile.equals("all") || profile.equals("chains")) chains(bridge);
                if (profile.equals("diagnostics")) diagnostics(bridge);
            }
        } finally {
            long beforeClose = System.nanoTime();
            bridge.close();
            long closed = System.nanoTime();
            long[] memoryAfter = probePath == null ? null : memorySnapshot();
            report.put("close_ns", closed - beforeClose);
            report.put("create_to_close_observed_ns", closed - start);
            if (memoryAfter != null) {
                report.put("memory_before_create", memory(memoryBefore));
                report.put("memory_after_close", memory(memoryAfter));
                // A newly established process maximum must lie in this interval.
                report.put("window_peak_rss_bytes", memoryAfter[1] > memoryBefore[1] ? memoryAfter[1] : null);
                report.put("window_peak_footprint_bytes", memoryAfter[6] > memoryBefore[6] ? memoryAfter[6] : null);
            }
            report.put("completed", true);
            report.put("stages", stages);
            report.put("features", features);
            report.put("api_errors", apiErrors);
            Files.writeString(Path.of(args[2]), json(report) + "\n");
        }
        if (apiErrors != 0) System.exit(2);
    }
}
