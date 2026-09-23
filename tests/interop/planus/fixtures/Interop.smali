.class public Linterop/Probe;
.super Ljava/lang/Object;
.source "Interop.java"

.annotation runtime Linterop/Marker;
    value = {0x7, 0x7fffffffffffffffL, "marker", true}
    child = .subannotation Linterop/Child;
        enabled = true
    .end subannotation
    raw = "nul\u0000pair\ud83d\ude00lone\ud800"
.end annotation

.field public static counter:I = 0x7

.method public static numbers()V
    .registers 8
    const/16 v0, 0x7
    const/16 v1, 0x8
    const/16 v2, -0x9
    const-wide v3, 0x7fffffffffffffffL
    const v5, 0x3fc00000
    const-wide v6, 0x4004000000000000L
    return-void
.end method

.method public static target()V
    .registers 1
    const-string v0, "token"
    const/4 v0, 0x7
    sput v0, Linterop/Probe;->counter:I
    sget v0, Linterop/Probe;->counter:I
    return-void
.end method

.method public static bits()V
    .registers 2
    const/high16 v0, -0x80000000
    const-wide/high16 v0, -0x8000000000000000L
    const-wide v0, 0x7ff8000000001234L
    const-wide v0, -0x10000000000000L
    return-void
.end method

.method public static caller()V
    .registers 0
    invoke-static {}, Linterop/Probe;->target()V
    return-void
.end method

.method public static params(ILjava/lang/String;)V
    .registers 2
    return-void
.end method

.method public static strings()V
    .registers 1
    const-string v0, "ascii"
    const-string v0, "bmp\u20ac"
    const-string v0, "nul\u0000end"
    const-string v0, "pair\ud83d\ude00"
    const-string v0, "lone\ud800"
    return-void
.end method

.method public static empty()V
    .registers 0
    return-void
.end method
