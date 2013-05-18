#ifndef LINUX_MM_INLINE_H
#define LINUX_MM_INLINE_H

#include <linux/huge_mm.h>

/**
 * page_is_file_cache - should the page be on a file LRU or anon LRU?
 * @page: the page to test
 *
 * Returns 1 if @page is page cache page backed by a regular filesystem,
 * or 0 if @page is anonymous, tmpfs or otherwise ram or swap backed.
 * Used by functions that manipulate the LRU lists, to sort a page
 * onto the right LRU list.
 *
 * We would like to get this info without a page flag, but the state
 * needs to survive until the page is last deleted from the LRU, which
 * could be as far down as __page_cache_release.
 */
static inline int page_is_file_cache(struct page *page)
{
	return !PageSwapBacked(page);
}

/* 해당 lru type의 lru 리스트에 page 등록 */
static __always_inline void add_page_to_lru_list(struct page *page,
				struct lruvec *lruvec, enum lru_list lru)
{
	 /* huge page 갯수 반환. 1인듯?  */
		/* HELPME: CONFIG_TRANSPARENT_HUGEPAGE와 관계가 큰듯 싶은데,
	  * 확실히 알려면 파봐야 함. */
	int nr_pages = hpage_nr_pages(page);
	/* lru type에 nr_pages 만큼 갱신 */
	mem_cgroup_update_lru_size(lruvec, lru, nr_pages);
	/* page->lru를 lru type list에 등록 */
	list_add(&page->lru, &lruvec->lists[lru]);
	/* vm_stat_diff에 nr_pages만큼을 갱신  */
	__mod_zone_page_state(lruvec_zone(lruvec), NR_LRU_BASE + lru, nr_pages);
}

/* 해당 lru type의 lru 리스트에 page 제거 */
static __always_inline void del_page_from_lru_list(struct page *page,
				struct lruvec *lruvec, enum lru_list lru)
{
	 /* huge page 갯수 반환. 1인듯?  */
	int nr_pages = hpage_nr_pages(page);
	/* lru type에 -nr_pages 만큼 갱신 */
	mem_cgroup_update_lru_size(lruvec, lru, -nr_pages);
	/* page를 list에서 제거 */
	list_del(&page->lru);
	/* vm_stat_diff에 -nr_pages만큼을 갱신  */
	__mod_zone_page_state(lruvec_zone(lruvec), NR_LRU_BASE + lru, -nr_pages);
}

/**
 * page_lru_base_type - which LRU list type should a page be on?
 * @page: the page to test
 *
 * Used for LRU list index arithmetic.
 *
 * Returns the base LRU type - file or anon - @page should be on.
 */
static inline enum lru_list page_lru_base_type(struct page *page)
{
	if (page_is_file_cache(page))
		return LRU_INACTIVE_FILE;
	return LRU_INACTIVE_ANON;
}

/**
 * page_off_lru - which LRU list was page on? clearing its lru flags.
 * @page: the page to test
 *
 * Returns the LRU list a page was on, as an index into the array of LRU
 * lists; and clears its Unevictable or Active flags, ready for freeing.
 */
/* PAGE lru type에 상관 없이 무조건 off시키고 lru type반환 */
static __always_inline enum lru_list page_off_lru(struct page *page)
{
	enum lru_list lru;

	/* UNEVICTABLE 페이지는 LRU에서 제거하면 안되는 것을 의미 */
	if (PageUnevictable(page)) {
		 /* HELPME: UNEVICTABLE이 지우면 안된다고 하면서, 왜 bit를 ᅠᆫclear하지???? */
		__ClearPageUnevictable(page);
		lru = LRU_UNEVICTABLE;
	} else {
		lru = page_lru_base_type(page);
		if (PageActive(page)) {
			__ClearPageActive(page);
			lru += LRU_ACTIVE;
		}
	}
	return lru;
}

/**
 * page_lru - which LRU list should a page be on?
 * @page: the page to test
 *
 * Returns the LRU list a page should be on, as an index
 * into the array of LRU lists.
 */
static __always_inline enum lru_list page_lru(struct page *page)
{
	enum lru_list lru;

	if (PageUnevictable(page))
		lru = LRU_UNEVICTABLE;
	else {
		lru = page_lru_base_type(page);
		if (PageActive(page))
			lru += LRU_ACTIVE;
	}
	return lru;
}

#endif
