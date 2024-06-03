1. ```c
   typedef struct Query
   {
   	NodeTag		type;
   
   	CmdType		commandType;	/* select|insert|update|delete|utility */
   
   	QuerySource querySource;	/* where did I come from? */
   
   	uint64		queryId;		/* query identifier (can be set by plugins) */
   
   	bool		canSetTag;		/* do I set the command result tag? */
   
   	Node	   *utilityStmt;	/* non-null if commandType == CMD_UTILITY */
   
   	int			resultRelation; /* rtable index of target relation for
   								 * INSERT/UPDATE/DELETE; 0 for SELECT */
   
   	bool		hasAggs;		/* has aggregates in tlist or havingQual */
   	bool		hasWindowFuncs; /* has window functions in tlist */
   	bool		hasTargetSRFs;	/* has set-returning functions in tlist */
   	bool		hasSubLinks;	/* has subquery SubLink */
   	bool		hasDistinctOn;	/* distinctClause is from DISTINCT ON */
   	bool		hasRecursive;	/* WITH RECURSIVE was specified */
   	bool		hasModifyingCTE;	/* has INSERT/UPDATE/DELETE in WITH */
   	bool		hasForUpdate;	/* FOR [KEY] UPDATE/SHARE was specified */
   	bool		hasRowSecurity; /* rewriter has applied some RLS policy */
   
   	bool		isReturn;		/* is a RETURN statement */
   
   	List	   *cteList;		/* WITH list (of CommonTableExpr's) */
   
   	List	   *rtable;			/* list of range table entries */
   	FromExpr   *jointree;		/* table join tree (FROM and WHERE clauses) */
   
   	List	   *targetList;		/* target list (of TargetEntry) */  描述目标列
   
   	OverridingKind override;	/* OVERRIDING clause */
   
   	OnConflictExpr *onConflict; /* ON CONFLICT DO [NOTHING | UPDATE] */
   
   	List	   *returningList;	/* return-values list (of TargetEntry) */
   
   	List	   *groupClause;	/* a list of SortGroupClause's */
   	bool		groupDistinct;	/* is the group by clause distinct? */
   
   	List	   *groupingSets;	/* a list of GroupingSet's if present */
   
   	Node	   *havingQual;		/* qualifications applied to groups */
   
   	List	   *windowClause;	/* a list of WindowClause's */
   
   	List	   *distinctClause; /* a list of SortGroupClause's */
   
   	List	   *sortClause;		/* a list of SortGroupClause's */
   
   	Node	   *limitOffset;	/* # of result tuples to skip (int8 expr) */
   	Node	   *limitCount;		/* # of result tuples to return (int8 expr) */
   	LimitOption limitOption;	/* limit type */
   
   	List	   *rowMarks;		/* a list of RowMarkClause's */
   
   	Node	   *setOperations;	/* set-operation tree if this is top level of
   								 * a UNION/INTERSECT/EXCEPT query */
   
   	List	   *constraintDeps; /* a list of pg_constraint OIDs that the query
   								 * depends on to be semantically valid */
   
   	List	   *withCheckOptions;	/* a list of WithCheckOption's (added
   									 * during rewrite) */
   
   	/*
   	 * The following two fields identify the portion of the source text string
   	 * containing this query.  They are typically only populated in top-level
   	 * Queries, not in sub-queries.  When not set, they might both be zero, or
   	 * both be -1 meaning "unknown".
   	 */
   	int			stmt_location;	/* start location, or -1 if unknown */
   	int			stmt_len;		/* length in bytes; 0 means "rest of string" */
   } Query;
   
     
   typedef struct TargetEntry
   {
   	Expr		xpr;
   	Expr	   *expr;			/* expression to evaluate */
   	AttrNumber	resno;			/* attribute number (see notes above) */
   	char	   *resname;		/* name of the column (could be NULL) */
   	Index		ressortgroupref;	/* nonzero if referenced by a sort/group
   									 * clause */  非0时表示在sort/group语句中的编号
   	Oid			resorigtbl;		/* OID of column's source table */   该列所属基表的oid
   	AttrNumber	resorigcol;		/* column's number in source table */  在源基表中的列编号
   	bool		resjunk;		/* set to true to eliminate the attribute from
   								 * final target list */
   } TargetEntry;
   
   
   struct PlannerInfo
   {
   	NodeTag		type;
   
   	Query	   *parse;			/* the Query being planned */  原始查询树
   
   	PlannerGlobal *glob;		/* global info for current planner run */  全局信息
   
   	Index		query_level;	/* 1 at the outermost Query */ 查询层次编号
   
   	PlannerInfo *parent_root;	/* NULL at outermost Query */
   
   	List	   *plan_params;	/* list of PlannerParamItems, see below */
   	Bitmapset  *outer_params;
   
   	struct RelOptInfo **simple_rel_array;	/* All 1-rel RelOptInfos */  所有基表信息的数组
   	int			simple_rel_array_size;	/* allocated size of array */
   
   	RangeTblEntry **simple_rte_array;	/* rangetable as an array */ 与simple_rel_array一样，不过保存RangeTblEntry
   
   	struct AppendRelInfo **append_rel_array;
   
   	Relids		all_baserels;
   
   	Relids		nullable_baserels;
   
   	List	   *join_rel_list;	/* list of join-relation RelOptInfos */  所有具有连接关系的基表信息
   	struct HTAB *join_rel_hash; /* optional hashtable for join relations */  为了加快查找join_rel_list而使用的hashtable
   
   	List	  **join_rel_level; /* lists of join-relation RelOptInfos */  当前处理层级信息
   	int			join_cur_level; /* index of list being extended */
   
   	List	   *init_plans;		/* init SubPlans for query */
   
   	List	   *cte_plan_ids;	/* per-CTE-item list of subplan IDs */
   
   	List	   *multiexpr_params;	/* List of Lists of Params for MULTIEXPR
   									 * subquery outputs */
   
   	List	   *eq_classes;		/* list of active EquivalenceClasses */
   
   	bool		ec_merging_done;	/* set true once ECs are canonical */
   
   	List	   *canon_pathkeys; /* list of "canonical" PathKeys */
   
   	List	   *left_join_clauses;	/* list of RestrictInfos for mergejoinable
   									 * outer join clauses w/nonnullable var on
   									 * left */
   
   	List	   *right_join_clauses; /* list of RestrictInfos for mergejoinable
   									 * outer join clauses w/nonnullable var on
   									 * right */
   
   	List	   *full_join_clauses;	/* list of RestrictInfos for mergejoinable
   									 * full join clauses */
   
   	List	   *join_info_list; /* list of SpecialJoinInfos */
   
   	Relids		all_result_relids;	/* set of all result relids */
   	Relids		leaf_result_relids; /* set of all leaf relids */
   
   	List	   *append_rel_list;	/* list of AppendRelInfos */
   
   	List	   *row_identity_vars;	/* list of RowIdentityVarInfos */
   
   	List	   *rowMarks;		/* list of PlanRowMarks */
   
   	List	   *placeholder_list;	/* list of PlaceHolderInfos */
   
   	List	   *fkey_list;		/* list of ForeignKeyOptInfos */
   
   	List	   *query_pathkeys; /* desired pathkeys for query_planner() */
   
   	List	   *group_pathkeys; /* groupClause pathkeys, if any */
   	List	   *window_pathkeys;	/* pathkeys of bottom window, if any */
   	List	   *distinct_pathkeys;	/* distinctClause pathkeys, if any */
   	List	   *sort_pathkeys;	/* sortClause pathkeys, if any */
   
   	List	   *part_schemes;	/* Canonicalised partition schemes used in the
   								 * query. */
   
   	List	   *initial_rels;	/* RelOptInfos we are now trying to join */
   
   	/* Use fetch_upper_rel() to get any particular upper rel */
   	List	   *upper_rels[UPPERREL_FINAL + 1]; /* upper-rel RelOptInfos */
   
   	/* Result tlists chosen by grouping_planner for upper-stage processing */
   	struct PathTarget *upper_targets[UPPERREL_FINAL + 1];
   
   	List	   *processed_tlist;
   
   	List	   *update_colnos;
   
   	/* Fields filled during create_plan() for use in setrefs.c */
   	AttrNumber *grouping_map;	/* for GroupingFunc fixup */
   	List	   *minmax_aggs;	/* List of MinMaxAggInfos */
   
   	MemoryContext planner_cxt;	/* context holding PlannerInfo */
   
   	double		total_table_pages;	/* # of pages in all non-dummy tables of
   									 * query */
   
   	double		tuple_fraction; /* tuple_fraction passed to query_planner */
   	double		limit_tuples;	/* limit_tuples passed to query_planner */
   
   	Index		qual_security_level;	/* minimum security_level for quals */
   	/* Note: qual_security_level is zero if there are no securityQuals */
   
   	bool		hasJoinRTEs;	/* true if any RTEs are RTE_JOIN kind */
   	bool		hasLateralRTEs; /* true if any RTEs are marked LATERAL */
   	bool		hasHavingQual;	/* true if havingQual was non-null */
   	bool		hasPseudoConstantQuals; /* true if any RestrictInfo has
   										 * pseudoconstant = true */
   	bool		hasAlternativeSubPlans; /* true if we've made any of those */
   	bool		hasRecursion;	/* true if planning a recursive WITH item */
   
   	/*
   	 * Information about aggregates. Filled by preprocess_aggrefs().
   	 */
   	List	   *agginfos;		/* AggInfo structs */
   	List	   *aggtransinfos;	/* AggTransInfo structs */
   	int			numOrderedAggs; /* number w/ DISTINCT/ORDER BY/WITHIN GROUP */
   	bool		hasNonPartialAggs;	/* does any agg not support partial mode? */
   	bool		hasNonSerialAggs;	/* is any partial agg non-serializable? */
   
   	/* These fields are used only when hasRecursion is true: */
   	int			wt_param_id;	/* PARAM_EXEC ID for the work table */
   	struct Path *non_recursive_path;	/* a path for non-recursive term */
   
   	/* These fields are workspace for createplan.c */
   	Relids		curOuterRels;	/* outer rels above current node */
   	List	   *curOuterParams; /* not-yet-assigned NestLoopParams */
   
   	/* optional private data for join_search_hook, e.g., GEQO */
   	void	   *join_search_private;
   
   	/* Does this query modify any partition key columns? */
   	bool		partColsUpdated;
   };
   ```

   