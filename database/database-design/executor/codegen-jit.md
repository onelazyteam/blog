```c
select funx(((1 + a.b) + a.a )/2) from a 

它的执行过程是
          expr(root)	// funroot
         funx(data)    // funx
       data (/) 2      // fundivision
   data (+) a.a        // funadd
1 (+) a.b              // funadd
```

可以看到，表达式越复杂,递归调用越深。执行的函数个数越多。

然而，在获取到查询计划之后再“写”代码，就简单了，只需要这样。

```c
llvm_expr_fun1
{
	tmp = 1 (+) a.b;
	tmp1 = tmp + a.a;
	tmp3 = tmp1 / 2;
	tmp4 = funx(tmp3); 
}
```

这么做的好处是：

1. 递归改顺序执行
2. 整个过程一个函数调用完成，性能提高明显。 整个优化方式适合 OLTP 和 OLAP 场景，我们可以像保存查询计划那样保存 LLVM 生成的表达式函数

### 参考文献

https://zhuanlan.zhihu.com/p/384478137

http://mysql.taobao.org/monthly/2016/11/10/

https://tetzank.github.io/posts/codegen-in-databases/

https://www.infoq.cn/article/cywqrf9usbg8dbzciidx

https://ericfu.me/code-gen-of-expression/

https://ericfu.me/code-gen-of-query/

http://mysql.taobao.org/monthly/2021/02/05/

