package org.luckypray.dexkit.smaliconsumer;

import java.nio.file.Files;
import java.nio.file.Path;
import org.luckypray.dexkit.DexKitBridge;
import org.luckypray.dexkit.result.MethodData;
import org.luckypray.dexkit.smali.*;

/** Executed after R8 shrinking, using the real JNI library and consumer rules. */
public final class SmaliConsumer {
    public static void main(String[] args) throws Exception {
        System.loadLibrary("dexkit");
        byte[] dex = Files.readAllBytes(Path.of(args[0]));
        MethodData method;
        String text;
        try (DexKitBridge bridge = DexKitBridge.create(new byte[][] {dex})) {
            method = bridge.getMethodData("Ltest/SmaliRoundTrip;->target()V");
            if (method == null) throw new AssertionError("Missing method");
            if (Boolean.getBoolean("smali.disabled")) {
                expectError(method, new SmaliOptions(), SmaliError.UNSUPPORTED);
                System.out.println("Feature-OFF consumer passed");
                return;
            }
            text = method.toSmali();
            if (!text.contains("return-void")) throw new AssertionError(text);
            expectError(method, new SmaliOptions(SmaliDebugMode.NONE, 1, 65536, 65536, 65536, 64),
                SmaliError.LIMIT_EXCEEDED);
        }
        if (!text.contains("return-void")) throw new AssertionError("Lost output after close");
        expectError(method, new SmaliOptions(), SmaliError.BRIDGE_CLOSED);
        System.out.println("R8 consumer success, native exception and closed-bridge checks passed");
    }

    private static void expectError(MethodData method, SmaliOptions options, SmaliError expected) {
        try {
            method.toSmali(options);
            throw new AssertionError("Missing " + expected);
        } catch (SmaliException error) {
            if (error.getError() != expected) throw new AssertionError(error);
        }
    }
}
