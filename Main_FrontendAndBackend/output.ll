; ModuleID = 'GoofyLang'
source_filename = "GoofyLang"

@strlit = private unnamed_addr constant [6 x i8] c"hellp\00", align 1
@.str_string = private constant [4 x i8] c"%s\0A\00"

define i32 @main() {
global:
  %i = alloca i32, align 4
  store i32 0, ptr %i, align 4
  br label %loopcond

loopcond:                                         ; preds = %loopbody, %global
  %i1 = load i32, ptr %i, align 4
  %loopcond2 = icmp slt i32 %i1, 10000000
  br i1 %loopcond2, label %loopbody, label %afterloop

loopbody:                                         ; preds = %loopcond
  %0 = call i32 (ptr, ...) @printf(ptr @.str_string, ptr @strlit)
  %inc = add i32 %i1, 1
  store i32 %inc, ptr %i, align 4
  br label %loopcond

afterloop:                                        ; preds = %loopcond
  ret i32 0
}

declare i32 @printf(ptr, ...)
