.class public Ltest/SmaliRoundTrip;
.super Ljava/lang/Object;
.source "RoundTrip.java"

.annotation runtime Ltest/Marker;
    value = {"hello\u0000\ud800", 0x7t, 0x8s, '\u20ac', true, Ljava/lang/String;}
    child = .subannotation Ltest/Child;
        value = 0x1.800000p1f
    .end subannotation
.end annotation

.field public static number:I = -123
.field public static wide:J = -9223372036854775808L
.field public static floatValue:F = -0.0f
.field public static doubleValue:D = 0x0.0000000000001p-1022
.field public static text:Ljava/lang/String; = "hello\u0000\ud83d\ude00"
.field private instance:I
    .annotation runtime Ltest/Marker;
    .end annotation
.end field

.method public constructor <init>()V
    .registers 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    return-void
.end method

.method public static target()V
    .registers 0
    return-void
.end method

.method public static branch(I)I
    .registers 3
    :begin
    const-wide/high16 v0, -0x8000000000000000L
    const/4 v0, -1
    if-eqz p0, :zero
    sparse-switch p0, :sparse
    goto :done
    :zero
    const/high16 v0, -0x80000000
    :done
    :end
    return v0
    :handler
    move-exception v0
    const/16 v0, -2
    return v0
    .catch Ljava/lang/Exception; {:begin .. :end} :handler
    .catchall {:begin .. :end} :handler
    :sparse
    .sparse-switch
        -1 -> :zero
        3 -> :done
    .end sparse-switch
.end method

.method public static array()[B
    .registers 2
    const/4 v0, 3
    new-array v0, v0, [B
    fill-array-data v0, :data
    return-object v0
    :data
    .array-data 1
        0xfft
        0x0t
        0x7ft
    .end array-data
.end method

.method public static modern(Ljava/lang/invoke/MethodHandle;)V
    .registers 2
    const-method-type v0, ()V
    const-method-handle v0, invoke-static@Ltest/SmaliRoundTrip;->target()V
    invoke-polymorphic {p0}, Ljava/lang/invoke/MethodHandle;->invokeExact([Ljava/lang/Object;)Ljava/lang/Object;, ()V
    invoke-polymorphic/range {p0 .. p0}, Ljava/lang/invoke/MethodHandle;->invokeExact([Ljava/lang/Object;)Ljava/lang/Object;, ()V
    invoke-static/range {}, Ltest/SmaliRoundTrip;->target()V
    invoke-custom {}, site("run", ()V, 42)@Ltest/Bootstrap;->bootstrap(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;I)Ljava/lang/invoke/CallSite;
    return-void
.end method
