; RUN: opt < %s -load-pass-plugin %S/../../bin/tc.* -passes="b-tc" -verify-dom-info -S | FileCheck %s

define void @test0(i32 %x) {
    %tmp = add i32 %x, 1
	call void @test0(i32 %tmp)
	ret void
}