## 特性描述

​	在典型的OLTP场景中，简单查询占了很大一部分比例。这种查询的特征是只涉及单表和简单表达式的查询，因此为了加速这类查询，提出了SQL-BY-PASS框架，在planner执行完毕后对这类查询做简单的模式判别，进入到特殊的执行路径里，跳过经典的执行器执行框架，包括算子的初始化与执行、表达式与投影等经典框架，直接重写一套简洁的执行路径，并且直接调用存储接口，这样可以大大加速简单查询的执行速度。

exec_simple_query中，优化器阶段后会进行sql bypass的判断:

```c
        /* SQL bypass */
        if (runOpfusionCheck) {
            (void)MemoryContextSwitchTo(oldcontext);
            void* opFusionObj = OpFusion::FusionFactory(
                OpFusion::getFusionType(NULL, NULL, plantree_list), oldcontext, NULL, plantree_list, NULL);
            if (opFusionObj != NULL) {
                ((OpFusion*)opFusionObj)->setCurrentOpFusionObj((OpFusion*)opFusionObj);
                if (OpFusion::process(FUSION_EXECUTE, NULL, completionTag, isTopLevel, NULL)) {
                    CommandCounterIncrement();
                    finish_xact_command();
                    EndCommand(completionTag, dest);
                    MemoryContextReset(OptimizerContext);
                    break;
                }
                Assert(0);
            }
            (void)MemoryContextSwitchTo(t_thrd.mem_cxt.msg_mem_cxt);
        }
```

