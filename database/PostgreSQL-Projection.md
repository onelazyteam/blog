## 投影函数

```c
static inline TupleTableSlot *
ExecProject(ProjectionInfo *projInfo)
{
	ExprContext *econtext = projInfo->pi_exprContext;
	ExprState  *state = &projInfo->pi_state;
	TupleTableSlot *slot = state->resultslot;
	bool		isnull;

	/*
	 * Clear any former contents of the result slot.  This makes it safe for
	 * us to use the slot's Datum/isnull arrays as workspace.
	 */
	ExecClearTuple(slot);

	/* Run the expression, discarding scalar result from the last column. */
	(void) ExecEvalExprSwitchContext(state, econtext, &isnull);

	/*
	 * Successfully formed a result row.  Mark the result slot as containing a
	 * valid virtual tuple (inlined version of ExecStoreVirtualTuple()).
	 */
	slot->tts_flags &= ~TTS_FLAG_EMPTY;
	slot->tts_nvalid = slot->tts_tupleDescriptor->natts;

	return slot;
}
```



## 用例

```sql
create table test(c1 int);
insert into test select generate_series(1, 10000);
select abs(c1) from test;
```



## 调用栈

```c
  * frame #0: 0x0000000102a2de94 postgres`ExecInterpExpr(state=0x00007fde9e858d40, econtext=0x00007fde9e858a58, isnull=0x00007ff7bd6b4126) at execExprInterp.c:514:6
    frame #1: 0x0000000102a2d3eb postgres`ExecInterpExprStillValid(state=0x00007fde9e858d40, econtext=0x00007fde9e858a58, isNull=0x00007ff7bd6b4126) at execExprInterp.c:1927:9
    frame #2: 0x0000000102a41627 postgres`ExecScan [inlined] ExecEvalExprSwitchContext(state=0x00007fde9e858d40, econtext=0x00007fde9e858a58, isNull=0x00007ff7bd6b4126) at executor.h:356:13
    frame #3: 0x0000000102a41603 postgres`ExecScan [inlined] ExecProject(projInfo=0x00007fde9e858d38) at executor.h:390:9
    frame #4: 0x0000000102a415ed postgres`ExecScan(node=0x00007fde9e858948, accessMtd=(postgres`SeqNext at nodeSeqscan.c:51), recheckMtd=(postgres`SeqRecheck at nodeSeqscan.c:90)) at execScan.c:236:12
    frame #5: 0x0000000102a6d564 postgres`ExecSeqScan(pstate=<unavailable>) at nodeSeqscan.c:112:9
    frame #6: 0x0000000102a3f02b postgres`ExecProcNodeFirst(node=0x00007fde9e858948) at execProcnode.c:464:9
    frame #7: 0x0000000102a3781a postgres`standard_ExecutorRun [inlined] ExecProcNode(node=0x00007fde9e858948) at executor.h:274:9
    frame #8: 0x0000000102a37805 postgres`standard_ExecutorRun [inlined] ExecutePlan(queryDesc=0x00007fde9e80d500, operation=CMD_SELECT, sendTuples=true, numberTuples=0, direction=<unavailable>, dest=0x00007fde9e8744f8) at execMain.c:1649:10
    frame #9: 0x0000000102a37795 postgres`standard_ExecutorRun(queryDesc=0x00007fde9e80d500, direction=<unavailable>, count=0, execute_once=<unavailable>) at execMain.c:361:3
    frame #10: 0x0000000102a376ce postgres`ExecutorRun(queryDesc=<unavailable>, direction=<unavailable>, count=<unavailable>, execute_once=<unavailable>) at execMain.c:307:3
    frame #11: 0x0000000102c23b41 postgres`PortalRunSelect(portal=0x00007fde9e836100, forward=<unavailable>, count=0, dest=0x00007fde9e8744f8) at pquery.c:922:4
    frame #12: 0x0000000102c23705 postgres`PortalRun(portal=0x00007fde9e836100, count=9223372036854775807, isTopLevel=true, run_once=<unavailable>, dest=0x00007fde9e8744f8, altdest=0x00007fde9e8744f8, qc=0x00007ff7bd6b43e0) at pquery.c:766:18
    frame #13: 0x0000000102c22671 postgres`exec_simple_query(query_string="select abs(c1) from test;") at postgres.c:1278:10
    frame #14: 0x0000000102c1fee3 postgres`PostgresMain(dbname=<unavailable>, username=<unavailable>) at postgres.c:0
    frame #15: 0x0000000102c1b489 postgres`BackendMain(startup_data=<unavailable>, startup_data_len=<unavailable>) at backend_startup.c:105:2
    frame #16: 0x0000000102b7a02b postgres`postmaster_child_launch(child_type=B_BACKEND, startup_data="", startup_data_len=4, client_sock=0x00007ff7bd6b5128) at launch_backend.c:277:3
    frame #17: 0x0000000102b7ea51 postgres`ServerLoop [inlined] BackendStartup(client_sock=0x00007ff7bd6b5128) at postmaster.c:3592:8
    frame #18: 0x0000000102b7e887 postgres`ServerLoop at postmaster.c:1674:6
    frame #19: 0x0000000102b7c17e postgres`PostmasterMain(argc=3, argv=0x0000600001439400) at postmaster.c:1372:11
    frame #20: 0x0000000102a8f169 postgres`main(argc=3, argv=0x0000600001439400) at main.c:199:3
    frame #21: 0x00007ff801f7c41f dyld`start + 1903
```

```
(lldb) p *projInfo

(ProjectionInfo) $0 = {
  type = T_ProjectionInfo
  pi_state = {                     // 表达式执行状态
    type = T_ExprState
    flags = '\x06'
    resnull = false
    resvalue = 0
    resultslot = 0x00007fde9e858ca8
    steps = 0x00007fde9e858e48
    evalfunc = 0x0000000102a2d3c0 (postgres`ExecInterpExprStillValid at execExprInterp.c:1916)
    expr = 0x00007fde9e8740f8
    evalfunc_private = 0x0000000102a2de80
    steps_len = 5
    steps_alloc = 16
    parent = 0x00007fde9e858948
    ext_params = NULL
    innermost_caseval = 0x0000000000000000
    innermost_casenull = 0x0000000000000000
    innermost_domainval = 0x0000000000000000
    innermost_domainnull = 0x0000000000000000
    escontext = NULL
  }
  pi_exprContext = 0x00007fde9e858a58   // 表达式的上下文信息
}

(lldb) p *projInfo->pi_exprContext
(ExprContext) $2 = {
  type = T_ExprContext
  ecxt_scantuple = 0x00007fde9e858b38    // 待处理的tuple
  ecxt_innertuple = NULL
  ecxt_outertuple = NULL
  ecxt_per_query_memory = 0x00007fde9e858600
  ecxt_per_tuple_memory = 0x00007fde9e879200
  ecxt_param_exec_vals = NULL
  ecxt_param_list_info = NULL
  ecxt_aggvalues = 0x0000000000000000
  ecxt_aggnulls = 0x0000000000000000
  caseValue_datum = 0
  caseValue_isNull = true
  domainValue_datum = 0
  domainValue_isNull = true
  ecxt_estate = 0x00007fde9e858700
  ecxt_callbacks = NULL
}

(lldb) p projInfo->pi_state
(ExprState) $3 = {
  type = T_ExprState
  flags = '\x06'
  resnull = false
  resvalue = 0
  resultslot = 0x00007fde9e858ca8
  steps = 0x00007fde9e858e48
  evalfunc = 0x0000000102a2d3c0 (postgres`ExecInterpExprStillValid at execExprInterp.c:1916)
  expr = 0x00007fde9e8740f8
  evalfunc_private = 0x0000000102a2de80
  steps_len = 5
  steps_alloc = 16
  parent = 0x00007fde9e858948
  ext_params = NULL
  innermost_caseval = 0x0000000000000000
  innermost_casenull = 0x0000000000000000
  innermost_domainval = 0x0000000000000000
  innermost_domainnull = 0x0000000000000000
  escontext = NULL
}


(lldb) p projInfo->pi_state.steps_len
(int) $13 = 5                                       // 分5步计算
(lldb) p/x projInfo->pi_state.steps[0].opcode
(intptr_t) $14 = 0x0000000102a2e030                 // EEOP_SCAN_FETCHSOME
(lldb) p/x projInfo->pi_state.steps[1].opcode
(intptr_t) $15 = 0x0000000102a2e180                 // EEOP_SCAN_VAR
(lldb) p/x projInfo->pi_state.steps[2].opcode
(intptr_t) $16 = 0x0000000102a2e540                 // EEOP_FUNCEXPR_STRICT
(lldb) p/x projInfo->pi_state.steps[3].opcode
(intptr_t) $17 = 0x0000000102a2e410                 // EEOP_ASSIGN_TMP
(lldb) p/x projInfo->pi_state.steps[4].opcode
(intptr_t) $18 = 0x0000000102a2ffeb                 // EEOP_DONE

(lldb)  p reverse_dispatch_table
(ExprEvalOpLookup[99]) $10 = {
  [0] = (opcode = 0x0000000102a2df00, op = EEOP_INNER_FETCHSOME)
  [1] = (opcode = 0x0000000102a2dfa0, op = EEOP_OUTER_FETCHSOME)
  [2] = (opcode = 0x0000000102a2e030, op = EEOP_SCAN_FETCHSOME)
  [3] = (opcode = 0x0000000102a2e0c0, op = EEOP_INNER_VAR)
  [4] = (opcode = 0x0000000102a2e120, op = EEOP_OUTER_VAR)
  [5] = (opcode = 0x0000000102a2e180, op = EEOP_SCAN_VAR)
  [6] = (opcode = 0x0000000102a2e1e0, op = EEOP_INNER_SYSVAR)
  [7] = (opcode = 0x0000000102a2e200, op = EEOP_OUTER_SYSVAR)
  [8] = (opcode = 0x0000000102a2e220, op = EEOP_SCAN_SYSVAR)
  [9] = (opcode = 0x0000000102a2e240, op = EEOP_WHOLEROW)
  [10] = (opcode = 0x0000000102a2e260, op = EEOP_ASSIGN_INNER_VAR)
  [11] = (opcode = 0x0000000102a2e2f0, op = EEOP_ASSIGN_OUTER_VAR)
  [12] = (opcode = 0x0000000102a2e380, op = EEOP_ASSIGN_SCAN_VAR)
  [13] = (opcode = 0x0000000102a2e410, op = EEOP_ASSIGN_TMP)
  [14] = (opcode = 0x0000000102a2e470, op = EEOP_ASSIGN_TMP_MAKE_RO)
  [15] = (opcode = 0x0000000102a2e4e0, op = EEOP_CONST)
  [16] = (opcode = 0x0000000102a2e510, op = EEOP_FUNCEXPR)
  [17] = (opcode = 0x0000000102a2e540, op = EEOP_FUNCEXPR_STRICT)
  [18] = (opcode = 0x0000000102a2e5a0, op = EEOP_FUNCEXPR_FUSAGE)
  [19] = (opcode = 0x0000000102a2e5f0, op = EEOP_FUNCEXPR_STRICT_FUSAGE)
  [20] = (opcode = 0x0000000102a2e674, op = EEOP_BOOL_AND_STEP_FIRST)
  [21] = (opcode = 0x0000000102a2e67f, op = EEOP_BOOL_AND_STEP)
  [22] = (opcode = 0x0000000102a2e6d0, op = EEOP_BOOL_AND_STEP_LAST)
  [23] = (opcode = 0x0000000102a2e716, op = EEOP_BOOL_OR_STEP_FIRST)
  [24] = (opcode = 0x0000000102a2e721, op = EEOP_BOOL_OR_STEP)
  [25] = (opcode = 0x0000000102a2e770, op = EEOP_BOOL_OR_STEP_LAST)
  [26] = (opcode = 0x0000000102a2e7b0, op = EEOP_BOOL_NOT_STEP)
  [27] = (opcode = 0x0000000102a2e7d0, op = EEOP_QUAL)
  [28] = (opcode = 0x0000000102a2e820, op = EEOP_JUMP)
  [29] = (opcode = 0x0000000102a2e840, op = EEOP_JUMP_IF_NULL)
  [30] = (opcode = 0x0000000102a2e870, op = EEOP_JUMP_IF_NOT_NULL)
  [31] = (opcode = 0x0000000102a2e8b0, op = EEOP_JUMP_IF_NOT_TRUE)
  [32] = (opcode = 0x0000000102a2e8f0, op = EEOP_NULLTEST_ISNULL)
  [33] = (opcode = 0x0000000102a2e910, op = EEOP_NULLTEST_ISNOTNULL)
  [34] = (opcode = 0x0000000102a2e940, op = EEOP_NULLTEST_ROWISNULL)
  [35] = (opcode = 0x0000000102a2e960, op = EEOP_NULLTEST_ROWISNOTNULL)
  [36] = (opcode = 0x0000000102a2e980, op = EEOP_BOOLTEST_IS_TRUE)
  [37] = (opcode = 0x0000000102a2e9b0, op = EEOP_BOOLTEST_IS_NOT_TRUE)
  [38] = (opcode = 0x0000000102a2ea00, op = EEOP_BOOLTEST_IS_FALSE)
  [39] = (opcode = 0x0000000102a2ea50, op = EEOP_BOOLTEST_IS_NOT_FALSE)
  [40] = (opcode = 0x0000000102a2ea80, op = EEOP_PARAM_EXEC)
  [41] = (opcode = 0x0000000102a2eaf0, op = EEOP_PARAM_EXTERN)
  [42] = (opcode = 0x0000000102a2eb10, op = EEOP_PARAM_CALLBACK)
  [43] = (opcode = 0x0000000102a2eb30, op = EEOP_CASE_TESTVAL)
  [44] = (opcode = 0x0000000102a2eb90, op = EEOP_DOMAIN_TESTVAL)
  [45] = (opcode = 0x0000000102a2ebf0, op = EEOP_MAKE_READONLY)
  [46] = (opcode = 0x0000000102a2ec30, op = EEOP_IOCOERCE)
  [47] = (opcode = 0x0000000102a2ed60, op = EEOP_IOCOERCE_SAFE)
  [48] = (opcode = 0x0000000102a2ed80, op = EEOP_DISTINCT)
  [49] = (opcode = 0x0000000102a2ee20, op = EEOP_NOT_DISTINCT)
  [50] = (opcode = 0x0000000102a2eeb0, op = EEOP_NULLIF)
  [51] = (opcode = 0x0000000102a2ef30, op = EEOP_SQLVALUEFUNCTION)
  [52] = (opcode = 0x0000000102a2ef44, op = EEOP_CURRENTOFEXPR)
  [53] = (opcode = 0x0000000102a2ef60, op = EEOP_NEXTVALUEEXPR)
  [54] = (opcode = 0x0000000102a2ef80, op = EEOP_ARRAYEXPR)
  [55] = (opcode = 0x0000000102a2efa0, op = EEOP_ARRAYCOERCE)
  [56] = (opcode = 0x0000000102a2f000, op = EEOP_ROW)
  [57] = (opcode = 0x0000000102a2f040, op = EEOP_ROWCOMPARE_STEP)
  [58] = (opcode = 0x0000000102a2f0e0, op = EEOP_ROWCOMPARE_FINAL)
  [59] = (opcode = 0x0000000102a2f1b0, op = EEOP_MINMAX)
  [60] = (opcode = 0x0000000102a2f1d0, op = EEOP_FIELDSELECT)
  [61] = (opcode = 0x0000000102a2f1f0, op = EEOP_FIELDSTORE_DEFORM)
  [62] = (opcode = 0x0000000102a2f210, op = EEOP_FIELDSTORE_FORM)
  [63] = (opcode = 0x0000000102a2f260, op = EEOP_SBSREF_SUBSCRIPTS)
  [64] = (opcode = 0x0000000102a2f2a0, op = EEOP_SBSREF_OLD)
  [65] = (opcode = 0x0000000102a2f2a0, op = EEOP_SBSREF_ASSIGN)
  [66] = (opcode = 0x0000000102a2f2a0, op = EEOP_SBSREF_FETCH)
  [67] = (opcode = 0x0000000102a2f2c0, op = EEOP_CONVERT_ROWTYPE)
  [68] = (opcode = 0x0000000102a2f2e0, op = EEOP_SCALARARRAYOP)
  [69] = (opcode = 0x0000000102a2f300, op = EEOP_HASHED_SCALARARRAYOP)
  [70] = (opcode = 0x0000000102a2f320, op = EEOP_DOMAIN_NOTNULL)
  [71] = (opcode = 0x0000000102a2f340, op = EEOP_DOMAIN_CHECK)
  [72] = (opcode = 0x0000000102a2f360, op = EEOP_XMLEXPR)
  [73] = (opcode = 0x0000000102a2f380, op = EEOP_JSON_CONSTRUCTOR)
  [74] = (opcode = 0x0000000102a2f3a0, op = EEOP_IS_JSON)
  [75] = (opcode = 0x0000000102a2f3c0, op = EEOP_JSONEXPR_PATH)
  [76] = (opcode = 0x0000000102a2f3e0, op = EEOP_JSONEXPR_COERCION)
  [77] = (opcode = 0x0000000102a2f400, op = EEOP_JSONEXPR_COERCION_FINISH)
  [78] = (opcode = 0x0000000102a2f420, op = EEOP_AGGREF)
  [79] = (opcode = 0x0000000102a2f470, op = EEOP_GROUPING_FUNC)
  [80] = (opcode = 0x0000000102a2f500, op = EEOP_WINDOW_FUNC)
  [81] = (opcode = 0x0000000102a2f560, op = EEOP_MERGE_SUPPORT_FUNC)
  [82] = (opcode = 0x0000000102a2f580, op = EEOP_SUBPLAN)
  [83] = (opcode = 0x0000000102a2f5b0, op = EEOP_AGG_STRICT_DESERIALIZE)
  [84] = (opcode = 0x0000000102a2f5d0, op = EEOP_AGG_DESERIALIZE)
  [85] = (opcode = 0x0000000102a2f640, op = EEOP_AGG_STRICT_INPUT_CHECK_ARGS)
  [86] = (opcode = 0x0000000102a2f690, op = EEOP_AGG_STRICT_INPUT_CHECK_NULLS)
  [87] = (opcode = 0x0000000102a2f6e0, op = EEOP_AGG_PLAIN_PERGROUP_NULLCHECK)
  [88] = (opcode = 0x0000000102a2f740, op = EEOP_AGG_PLAIN_TRANS_INIT_STRICT_BYVAL)
  [89] = (opcode = 0x0000000102a2f8a0, op = EEOP_AGG_PLAIN_TRANS_STRICT_BYVAL)
  [90] = (opcode = 0x0000000102a2f9a0, op = EEOP_AGG_PLAIN_TRANS_BYVAL)
  [91] = (opcode = 0x0000000102a2fa80, op = EEOP_AGG_PLAIN_TRANS_INIT_STRICT_BYREF)
  [92] = (opcode = 0x0000000102a2fc30, op = EEOP_AGG_PLAIN_TRANS_STRICT_BYREF)
  [93] = (opcode = 0x0000000102a2fd70, op = EEOP_AGG_PLAIN_TRANS_BYREF)
  [94] = (opcode = 0x0000000102a2fea0, op = EEOP_AGG_PRESORTED_DISTINCT_SINGLE)
  [95] = (opcode = 0x0000000102a2fef0, op = EEOP_AGG_PRESORTED_DISTINCT_MULTI)
  [96] = (opcode = 0x0000000102a2ff50, op = EEOP_AGG_ORDERED_TRANS_DATUM)
  [97] = (opcode = 0x0000000102a2ff90, op = EEOP_AGG_ORDERED_TRANS_TUPLE)
  [98] = (opcode = 0x0000000102a2ffeb, op = EEOP_DONE)
}
(lldb) p/x projInfo->pi_state.steps[0].opcode
(intptr_t) $11 = 0x0000000102a2e030
```

​	表达式执行时，通过state->evalfunc函数完成具体的表达式计算，具体的计算流程放在steps中，其中每个steps的opcode需要通过`reverse_dispatch_table`来确定当前计算走`ExecInterpExpr`的哪个分支。



上述project计算分了5步：

##### 第一步：EEOP_SCAN_FETCHSOME

从`projInfo->pi_exprContext->ecxt_scantuple`读取到scantuple

```c
(lldb) p *projInfo->pi_exprContext->ecxt_scantuple
(TupleTableSlot) $22 = {
  type = T_TupleTableSlot
  tts_flags = 16
  tts_nvalid = 0
  tts_ops = 0x0000000103097898
  tts_tupleDescriptor = 0x00007fdea2833fe8
  tts_values = 0x00007fde9e858ba8
  tts_isnull = 0x00007fde9e858bb0
  tts_mcxt = 0x00007fde9e858600
  tts_tid = {
    ip_blkid = (bi_hi = 0, bi_lo = 0)
    ip_posid = 1
  }
  tts_tableOid = 16388
}

// 相关代码
		EEO_CASE(EEOP_SCAN_FETCHSOME)
		{
			CheckOpSlotCompatibility(op, scanslot);

			slot_getsomeattrs(scanslot, op->d.fetch.last_var);

			EEO_NEXT();
		}

```

##### 第二步：EEOP_SCAN_VAR

从代码中可以看到输入为`op->d.var.attnum`，并将结果放入resvalue中

```c
// 从扫描行中拿第0列，结果放到resvalue中
(lldb) p projInfo->pi_state.steps[1].d.fetch
(ExprEvalStep) $23 = {
  opcode = 4339196288
  resvalue = 0x00007fde9e8592c8
  resnull = 0x00007fde9e8592d0
  d = {
    fetch = {
      last_var = 0
      fixed = true
      known_desc = NULL
      kind = NULL
    }
    var = (attnum = 0, vartype = 23)

  // 相关代码
	EEO_CASE(EEOP_SCAN_VAR)
		{
			int			attnum = op->d.var.attnum;

			/* See EEOP_INNER_VAR comments */

			Assert(attnum >= 0 && attnum < scanslot->tts_nvalid);
			*op->resvalue = scanslot->tts_values[attnum];
			*op->resnull = scanslot->tts_isnull[attnum];

			EEO_NEXT();
		}
```

##### 第三步：EEOP_FUNCEXPR_STRICT

输入：

```c
(lldb) p projInfo->pi_state.steps[2].d.func
(ExprEvalStep::(unnamed struct)) $26 = {
  finfo = 0x00007fde9e859258
  fcinfo_data = 0x00007fde9e8592a8
  fn_addr = 0x0000000102c907d0 (postgres`int4abs at int.c:1192)
  nargs = 1
  make_ro = false
}
```

执行代码：

```c
		EEO_CASE(EEOP_FUNCEXPR_STRICT)
		{
			FunctionCallInfo fcinfo = op->d.func.fcinfo_data;
			NullableDatum *args = fcinfo->args;
			int			nargs = op->d.func.nargs;
			Datum		d;

			/* strict function, so check for NULL args */
			for (int argno = 0; argno < nargs; argno++)
			{
				if (args[argno].isnull)
				{
					*op->resnull = true;
					goto strictfail;
				}
			}
			fcinfo->isnull = false;
			d = op->d.func.fn_addr(fcinfo);
			*op->resvalue = d;
			*op->resnull = fcinfo->isnull;

	strictfail:
			EEO_NEXT();
		}
```

##### 第四步：EEOP_ASSIGN_TMP

```c
		EEO_CASE(EEOP_ASSIGN_TMP)
		{
			int			resultnum = op->d.assign_tmp.resultnum;

			Assert(resultnum >= 0 && resultnum < resultslot->tts_tupleDescriptor->natts);
			resultslot->tts_values[resultnum] = state->resvalue;
			resultslot->tts_isnull[resultnum] = state->resnull;

			EEO_NEXT();
		}
```

##### 第四步：EEOP_DONE

结束。