Linux kernel Study Group(iamroot x86-8 beginner)
========================================
* 1차 교재 = [리눅스 커널의 내부구조](http://www.yes24.com/24/goods/3080849)
* 2차 교재 = [만들면서 배우는 OS 커널의 구조와 원리](http://www.yes24.com/24/goods/1469757)
* 부트로더 = [LILO 23.2](https://github.com/x86-8/lilo232.git)
* 커널분석 = [Linux 3.2](https://github.com/x86-8/linux-3.2.git) setup_arch() 이후부터 v3.7로 변경
* 커널분석 = [Linux 3.7](https://github.com/x86-8/linux-3.7.git)
* 문서고 = [x86-8-docs](https://github.com/x86-8/x86-8-docs.git)

Study history (날짜:인원:내용)
==============================
87. 2012/12/22:??:*init/main.c* -
88. 2013/01/05:03:*init/main.c* - setup_per_cpu_areas | 누리꿈에서 스터디
*   2013/01/12:02: 장소 예약 취소로 스터디 못함
89. 2013/01/19:02:*init/main.c* - setup_per_cpu_areas - pcpu_embed_first_chunk - pcpu_build_alloc_info | 복잡한 percpu
90. 2013/01/26:02:*init/main.c* - setup_per_cpu_areas - pcpu_embed_first_chunk - pcpu_build_alloc_info | @minq님의 linux 환경으로 분석
91. 2013/02/02:03:*init/main.c* - setup_per_cpu_areas - pcpu_embed_first_chunk - pcpu_build_alloc_info | @Florentius님 복귀
92. 2013/02/16:04:*init/main.c* - setup_per_cpu_areas - pcpu_embed_first_chunk - pcpu_build_alloc_info | @jminrang님 복귀
93. 2013/02/23:03:*init/main.c* - setup_per_cpu_areas - pcpu_embed_first_chunk
94. 2013/03/02:04:*init/main.c* - setup_per_cpu_areas - pcpu_embed_first_chunk - pcpu_setup_first_chunk | 2주만 더하면 되려나
95. 2013/03/09:02:*init/main.c* - setup_per_cpu_areas - pcpu_page_first_chunk
96. 2013/03/16:04:*init/main.c* - setup_per_cpu_ares | setup_per_cpu_ares() 종료
97. 2013/03/23:04:*init/main.c* - 4명 왔으나, 스터디 진전없음
98. 2013/04/20:03:*init/main.c* - build_all_zonelists - __build_all_zonelists - build_zonelists
99. 2013/04/27:03:*init/main.c* - build_all_zonelists - __build_all_zonelists - build_zonelist_cache
100. 2013/05/04:03:*init/main.c* - build_all_zonelists - __build_all_zonelists - local_memory_node
101. 2013/05/11:02:*init/main.c* - page_alloc_init - page_alloc_cpu_notify
102. 2013/05/18:02:*init/main.c* - page_alloc_init - page_alloc_cpu_notify - lru_add_drain_cpu - __pagevec_lru_add - pagevec_lru_move_fn - release_pages - free_hot_cold_page_list - free_hot_cold_page - free_pages_prepare
103. 2013/05/25:02:*init/main.c* - page_alloc_init - page_alloc_cpu_notify - lru_add_drain_cpu - __pagevec_lru_add - pagevec_lru_move_fn - release_pages - free_hot_cold_page_list - free_hot_cold_page - get_pageblock_migratetype - get_pageblock_flags_group - pfn_to_bitidx
104. 2013/06/01:03:*init/main.c* - parse_args
