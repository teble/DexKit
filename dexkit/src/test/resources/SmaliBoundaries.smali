.class public Ltest/SmaliBoundaries;
.super Ljava/lang/Object;

.method public static constants()V
    .registers 256
    const/4 v15, -8
    const/16 v255, -32768
    const v255, -2147483648
    const/high16 v255, -2147483648
    const-wide/16 v254, -32768L
    const-wide/32 v254, -2147483648L
    const-wide v254, -9223372036854775808L
    const-wide/high16 v254, -9223372036854775808L
    return-void
.end method

.method public static range()V
    .registers 65535
    move-wide/16 v65533, v65533
    invoke-static/range {v65530 .. v65534}, Lother/Target;->five(IIIII)V
    invoke-static/range {}, Lother/Target;->zero()V
    return-void
.end method

.method public static polymorphic()V
    .registers 5
    invoke-polymorphic {v0, v1, v2, v3, v4}, Ljava/lang/invoke/MethodHandle;->invokeExact([Ljava/lang/Object;)Ljava/lang/Object;, (IIII)V
    return-void
.end method

.method public static arrays()V
    .registers 1
    fill-array-data v0, :two
    fill-array-data v0, :four
    fill-array-data v0, :eight
    return-void
    :two
    .array-data 2
        -32768s
        32767s
    .end array-data
    :four
    .array-data 4
        -2147483648
        2147483647
    .end array-data
    :eight
    .array-data 8
        -9223372036854775808L
        9223372036854775807L
    .end array-data
.end method

.method public static nullableLocal()V
    .registers 1
    .local v0, null:V
    .source
    .line 1
    .line -1
    return-void
    .end local v0
.end method
