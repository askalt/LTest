; ModuleID = 'test.cpp'
source_filename = "test.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.Promise = type { i32, ptr, i32 }

@x = dso_local global i32 1, align 4

; Устаналивает дочернюю корутину промису.
declare dso_local void @set_child_hdl(ptr noundef %0, ptr noundef %1);

; Устанавливает промису значения возврата.
declare dso_local void @set_result(ptr noundef %0, i32 noundef %1);

; Возвращает из промиса результат последней дочерней корутины.
; Планировщик выставит этот результат, когда она завершится.
declare dso_local i32 @get_child_return(ptr %0);

; [!] Некоторая init функция для начальной инициализации тестовых данных.
; Например, создание входных классов.
define void @init_func() {
  store i32 1, i32* @x
  ret void
}

define dso_local i8* @add() #0 presplitcoroutine {
entry:
  %promise = alloca %struct.Promise
  %id = call token @llvm.coro.id(i32 0, ptr %promise, ptr null, ptr null)
  %size = call i32 @llvm.coro.size.i32()
  %alloc = call i8* @malloc(i32 %size)
  %hdl = call noalias i8* @llvm.coro.begin(token %id, i8* %alloc)
  br label %execution.init

execution.init:
  call void @debug_print(i32 2);
  %0 = alloca i32
  %suspend.0 = call i8 @llvm.coro.suspend(token none, i1 false)
  switch i8 %suspend.0, label %suspend [i8 0, label %execution.1
                                       i8 1, label %cleanup]

execution.1:
  call void @debug_print(i32 3);
  %1 = alloca i32, align 4
  %2 = load i32, ptr @x, align 4
  %suspend.1 = call i8 @llvm.coro.suspend(token none, i1 false)
  switch i8 %suspend.1, label %suspend [i8 0, label %execution.2
                                       i8 1, label %cleanup]

execution.2:
  call void @debug_print(i32 4);
  store i32 %2, ptr %1, align 4
  %suspend.2 = call i8 @llvm.coro.suspend(token none, i1 false)
  switch i8 %suspend.2, label %suspend [i8 0, label %execution.3
                                       i8 1, label %cleanup]

execution.3:
  call void @debug_print(i32 5);
  %3 = load i32, ptr %1, align 4
  %4 = add nsw i32 %3, 1
  %suspend.3 = call i8 @llvm.coro.suspend(token none, i1 false)
  switch i8 %suspend.3, label %suspend [i8 0, label %execution.4
                                       i8 1, label %cleanup]

execution.4:
  call void @debug_print(i32 6);
  store i32 %4, ptr %1, align 4
  %suspend.4 = call i8 @llvm.coro.suspend(token none, i1 false)
  switch i8 %suspend.4, label %suspend [i8 0, label %execution.5
                                       i8 1, label %cleanup]

execution.5:
  call void @debug_print(i32 7);
  %5 = load i32, ptr %1, align 4
  store i32 %5, ptr @x, align 4
  %suspend.5 = call i8 @llvm.coro.suspend(token none, i1 false)
  switch i8 %suspend.5, label %suspend [i8 0, label %execution.retblock
                                       i8 1, label %cleanup]

execution.retblock:
  call void @debug_print(i32 8);
   ; сделаем вид что void это возврат -1.
  call void @set_result(ptr %promise, i32 -1)
  %suspend.ret = call i8 @llvm.coro.suspend(token none, i1 false)
  switch i8 %suspend.ret, label %suspend [i8 0, label %cleanup
                                       i8 1, label %cleanup]

cleanup:
  %mem = call i8* @llvm.coro.free(token %id, i8* %hdl)
  call void @free(i8* %mem)
  br label %suspend

suspend:
  %unused = call i1 @llvm.coro.end(i8* %hdl, i1 false, token none)
  ret i8* %hdl
}

define dso_local i8* @get() #0 presplitcoroutine {
entry:
  %promise = alloca %struct.Promise
  %id = call token @llvm.coro.id(i32 0, ptr %promise, ptr null, ptr null)
  %size = call i32 @llvm.coro.size.i32()
  %alloc = call i8* @malloc(i32 %size)
  %hdl = call noalias i8* @llvm.coro.begin(token %id, i8* %alloc)
  br label %execution.init

execution.init:
 call void @debug_print(i32 10);
  %0 = alloca i32
  %suspend.0 = call i8 @llvm.coro.suspend(token none, i1 false)
  switch i8 %suspend.0, label %suspend [i8 0, label %execution.1
                                       i8 1, label %cleanup]
execution.1:
  call void @debug_print(i32 11);
  %1 = load i32, ptr @x, align 4
  %suspend.1 = call i8 @llvm.coro.suspend(token none, i1 false)
  switch i8 %suspend.1, label %suspend [i8 0, label %execution.retblock
                                       i8 1, label %cleanup]

execution.retblock:
  call void @debug_print(i32 12);
  call void @set_result(ptr %promise, i32 %1)
  %suspend.ret = call i8 @llvm.coro.suspend(token none, i1 false)
  switch i8 %suspend.ret, label %suspend [i8 0, label %cleanup
                                       i8 1, label %cleanup]

; [!] Генерируется cleanup block.
cleanup:
  %mem = call i8* @llvm.coro.free(token %id, i8* %hdl)
  call void @free(i8* %mem)
  br label %suspend

; [!] Генерируется suspend block
suspend:
  %unused = call i1 @llvm.coro.end(i8* %hdl, i1 false, token none)
  ret i8* %hdl
}

; Функция void @test_adder() эволюционирует в корутину.
; [!] Аттрибут presplitcoroutine.
define dso_local i8* @test_adder() #0 presplitcoroutine {
; [!] entry block.
entry:
  %promise = alloca %struct.Promise
  %id = call token @llvm.coro.id(i32 0, ptr %promise, ptr null, ptr null)
  %size = call i32 @llvm.coro.size.i32()
  %alloc = call i8* @malloc(i32 %size)
  %hdl = call noalias i8* @llvm.coro.begin(token %id, i8* %alloc)
  br label %execution.init

; [!] Первое прерывание происходит на старте в этом блоке.
execution.init:
  call void @debug_print(i32 0)
  %suspend.0 = call i8 @llvm.coro.suspend(token none, i1 false)
  switch i8 %suspend.0, label %suspend [i8 0, label %execution.1
                                       i8 1, label %cleanup]

; [!] Основное исполнение заключается в execution block'и.
execution.1:
  call void @debug_print(i32 1)
  %0 = alloca i32, align 4
  store i32 0, ptr %0, align 4
  ; [!] Вызов функции, для которой мы тоже генерировали корутину.
  ; Получаем дочерний хэндлер.
  %add_hdl.0 = call i8* @add()
  ; Устанавливаем дочерний хэндлер, чтобы планировщик мог понять,
  ; что корутина завела ребенка.
  call void @set_child_hdl(ptr %promise, ptr %add_hdl.0)
  ; Дальше прерываемся. Планировщик разбудит нас, когда дочерняя корутина завершится.
  ; [!] suspension point.
  %suspend.1 = call i8 @llvm.coro.suspend(token none, i1 false)
  switch i8 %suspend.1, label %suspend [i8 0, label %execution.2
                                       i8 1, label %cleanup]

execution.2:
  call void @debug_print(i32 9)
  ; [!] Снова вызов функции, но теперь с возвращаемым значением.
  %get_hdl.0 = call i8* @get()
  call void @set_child_hdl(ptr %promise, ptr %get_hdl.0)
  %suspend.2 = call i8 @llvm.coro.suspend(token none, i1 false)
  switch i8 %suspend.2, label %suspend [i8 0, label %execution.retblock
                                       i8 1, label %cleanup]

; [!] retblock, где устанавливаем результат исполнения.
execution.retblock:
  call void @debug_print(i32 13);
  ; [!] Получаем результат работы дочерней корутины.
  ; Планировщик проставляет его по ее завершению.
  %1 = call i32 @get_child_return(ptr %promise)
  call void @set_result(ptr %promise, i32 %1)
  %suspend.ret = call i8 @llvm.coro.suspend(token none, i1 false)
  switch i8 %suspend.ret, label %suspend [i8 0, label %cleanup
                                       i8 1, label %cleanup]

; [!] cleanup block.
cleanup:
  %mem = call i8* @llvm.coro.free(token %id, i8* %hdl)
  call void @free(i8* %mem)
  br label %suspend

; [!] suspend block.
suspend:
  %unused = call i1 @llvm.coro.end(i8* %hdl, i1 false, token none)
  ret i8* %hdl
}

; [!] main.
define i32 @main() {
  ; [!] Вызов функции тестирования.
  call void @test_task(ptr @test_adder)
  ret i32 0
}

; [!] Функция, которая тестирует таски из executor.
declare void @test_task(ptr)

; Можно не генерировать эти объявления.
; Они добавлены для того, чтобы собрать пример.
declare void @free(i8* %mem)
declare i8* @malloc(i32 %size)
declare void @debug_print(i32)

; [!] llvm coro интринсики.
declare token @llvm.coro.id(i32, ptr, ptr, ptr)
declare i1 @llvm.coro.alloc(token)
declare i32 @llvm.coro.size.i32()
declare ptr @llvm.coro.begin(token, ptr)
declare i8 @llvm.coro.suspend(token, i1)
declare ptr @llvm.coro.free(token, ptr)
declare i1 @llvm.coro.end(ptr, i1, token)
declare void @llvm.coro.resume(ptr)
declare void @llvm.coro.destroy(ptr)

declare void @llvm.memcpy.p0.p0.i64(ptr noalias nocapture writeonly, ptr noalias nocapture readonly, i64, i1 immarg) #1

attributes #0 = { mustprogress noinline nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nocallback nofree nounwind willreturn memory(argmem: readwrite) }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"clang version 18.0.0 (https://github.com/llvm/llvm-project.git fcf5ac84a62de48fc9a6be545e84ccd7e6436320)"}
