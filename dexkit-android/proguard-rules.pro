-keepclasseswithmembers,includedescriptorclasses class org.luckypray.dexkit.** {
    native <methods>;
}

# Native smali errors construct this exception directly with stable numeric fields.
-keep class org.luckypray.dexkit.smali.SmaliException {
    public <init>(int, int, long, int, long, long, long, long);
}
