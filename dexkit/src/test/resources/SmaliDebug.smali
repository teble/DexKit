.class public Ltest/SmaliDebug;
.super Ljava/lang/Object;
.source "Debug.java"

.method public debug(JI)V
    .registers 5
    .param p1, "wide"
    .param p3, "other"
        .annotation runtime Ltest/Marker;
        .end annotation
    .end param
    .prologue
    .line 10
    const/4 v0, 0
    .local v0, "temp":I
    .line 11
    add-int/lit8 v0, v0, 1
    .end local v0
    .restart local v0
    .source "Inline.java"
    .epilogue
    .line 12
    return-void
    .end local v0
.end method

.method public static shortMethod()V
    .registers 0
    return-void
.end method

.method public static longMethod()V
    .registers 1
    .line 1
    const/4 v0, 0
    .line 2
    const/4 v0, 1
    .line 3
    const/4 v0, 2
    .line 4
    return-void
.end method
