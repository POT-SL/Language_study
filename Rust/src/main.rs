use std::arch::aarch64::{float32x2_t, float32x2x3_t};

fn main() {
    
    //
    // 第零章 变量和print
    //

    // print!是输出（不换行）
    print!("Hello!");
    // \n换行
    print!("\n");
    // println!是换行输出
    println!("Welcome to, Rust!");

    // 使用let创建常量
    let a = 5;
    // 使用let mut创建变量（类型自动判断）
    let mut b = 10;
    // 常量和变量都可以先创建再赋值, 但是常量赋值后不可更改
    // 
    let a;
    let mut b;
    a = 5;
    b = 10;
    // a = 20;  // (X) 不能更改常量c的值
    b = 20; // 可以更改变量d的值

    // rust中有多种原生变量类型：
    // int, uint, float, char, bool, tuple, array.
    // int有i8,i16,i32,i64,i128,isize
    // uint（无符号int）有u8,u16,u32,u64,u128,usize
    // （int，uint创建时根据系统架构判断，x32则为i32/u32，x64则为i64/u64）
    let a: isize = -10;
    let mut b: usize = 10;
    // float有f32,f64
    let c: f64 = 3.14;
    // char是单字符类型，使用单引号''
    let d: char = 'D';
    // bool是布尔类型，只会有true或false俩个值
    // tuple是元组类型，可以存放多个不同类型的值
    // array是数组类型，存放多个相同类型的值



}