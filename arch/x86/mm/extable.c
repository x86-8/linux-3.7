#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/sort.h>
#include <asm/uaccess.h>

/* exception table의 insn 주소 반환 */
static inline unsigned long
ex_insn_addr(const struct exception_table_entry *x)
{
	 /* insn에 있는 값은 offset이기 때문에, 절대주소로 변환 */
	return (unsigned long)&x->insn + x->insn;
}
/* exception table의 fixup 주소 반환 */
static inline unsigned long
ex_fixup_addr(const struct exception_table_entry *x)
{
	 /* fixup에 있는 값은 offset이기 때문에, 절대주소로 변환 */
	return (unsigned long)&x->fixup + x->fixup;
}

/* exception을 고친다 */
int fixup_exception(struct pt_regs *regs)
{
	const struct exception_table_entry *fixup;
	unsigned long new_ip;

	/* Plug and Play BIOS를 나타내며 기본값은 No */
#ifdef CONFIG_PNPBIOS
	if (unlikely(SEGMENT_IS_PNP_CODE(regs->cs))) {
		extern u32 pnp_bios_fault_eip, pnp_bios_fault_esp;
		extern u32 pnp_bios_is_utter_crap;
		pnp_bios_is_utter_crap = 1;
		printk(KERN_CRIT "PNPBIOS fault.. attempting recovery.\n");
		__asm__ volatile(
			"movl %0, %%esp\n\t"
			"jmp *%1\n\t"
			: : "g" (pnp_bios_fault_esp), "g" (pnp_bios_fault_eip));
		panic("do_trap: can't hit this");
	}
#endif

	/* ip 주소에 entry를 찾는다 */
	fixup = search_exception_tables(regs->ip);
	if (fixup) {
		 /* entry에서 새로운 ip를 가져온다 */
		new_ip = ex_fixup_addr(fixup);

		/* kernel symbol은 2GB 이내에서만 접근 가능하다. insn부터 실제
		 * fixup의 사이가 2GB-4가 넘는 경우. */
		if (fixup->fixup - fixup->insn >= 0x7ffffff0 - 4) {
			/* Special hack for uaccess_err */
			 /* HELPME: uaccess_err가 user access error를 의미하는
			  * 듯... 차이나는 만큼(fixup-insn)을 줄이지 않고, 다시
			  * 2GB만큼을 줄이는 것을 보면, 정상적으로 fixup이 된다는
			  * 말인데, 혹시 매핑하는 영역이 있는지 모르겠음. */
			current_thread_info()->uaccess_err = 1;
			/* 2GB만큼을 줄인다 */
			/* HELPME: 차이가 2GB보다 클때, 2GB영역만큼을 줄이면,
			 * 어쨌든 new_ip는 fixup의 위치보다 나중 위치를 가리키게
			 * 되는데, 이것을 고쳤다고 표현해야 하나? */
			new_ip -= 0x7ffffff0;
		}
		/* 새로운 ip를 반영하고, fix up 과정을 마침 */
		regs->ip = new_ip;
		return 1;
	}

	/* fix 못함 */
	return 0;
}

/* Restricted version used during very early boot */
int __init early_fixup_exception(unsigned long *ip)
{
	const struct exception_table_entry *fixup;
	unsigned long new_ip;

	fixup = search_exception_tables(*ip);
	if (fixup) {
		new_ip = ex_fixup_addr(fixup);

		if (fixup->fixup - fixup->insn >= 0x7ffffff0 - 4) {
			/* uaccess handling not supported during early boot */
			return 0;
		}

		*ip = new_ip;
		return 1;
	}

	return 0;
}

/*
 * Search one exception table for an entry corresponding to the
 * given instruction address, and return the address of the entry,
 * or NULL if none is found.
 * We use a binary search, and thus we assume that the table is
 * already sorted.
 */
/* exception table을 2진검색으로 탐색, 찾지 못할 경우 NULL, 찾은 경우,
 * 찾은 exception_table반환 */
const struct exception_table_entry *
search_extable(const struct exception_table_entry *first,
	       const struct exception_table_entry *last,
	       unsigned long value)
{
	while (first <= last) {
		const struct exception_table_entry *mid;
		unsigned long addr;

		mid = ((last - first) >> 1) + first;
		addr = ex_insn_addr(mid);
		if (addr < value)
			first = mid + 1;
		else if (addr > value)
			last = mid - 1;
		else
			return mid;
        }
        return NULL;
}

/*
 * The exception table needs to be sorted so that the binary
 * search that we use to find entries in it works properly.
 * This is used both for the kernel exception table and for
 * the exception tables of modules that get loaded.
 *
 */
/* 커널 exception table과 모듈의 exception table 정렬 시에 사용  */
static int cmp_ex(const void *a, const void *b)
{
	const struct exception_table_entry *x = a, *y = b;

	/*
	 * This value will always end up fittin in an int, because on
	 * both i386 and x86-64 the kernel symbol-reachable address
	 * space is < 2 GiB.
	 *
	 * This compare is only valid after normalization.
	 */
	/* 커널 symbol이 닿을수 있는 주소 영역은 2GB 이하라는 군... */
	return x->insn - y->insn;
}

/* 커널 exception table 정렬 */
void sort_extable(struct exception_table_entry *start,
		  struct exception_table_entry *finish)
{
	struct exception_table_entry *p;
	int i;

	/* Convert all entries to being relative to the start of the section */
	/* 모든 entries를 normalize 한다. */
	/* 순차로 만드는 것은 insn이 같은 entry가 존재할 수 있기 때문. */
	i = 0;
	for (p = start; p < finish; p++) {
		p->insn += i;
		i += 4;
		/* HELPME: fixup은 정렬 과정에 안 쓰니 필요 없을 텐데... 왜? */
		p->fixup += i;
		i += 4;
	}

	/* exception table을 실제로 정렬 */
	sort(start, finish - start, sizeof(struct exception_table_entry),
	     cmp_ex, NULL);

	/* Denormalize all entries */
	/* 모든 entries를 denormalize 한다 */
	i = 0;
	for (p = start; p < finish; p++) {
		p->insn -= i;
		i += 4;
		p->fixup -= i;
		i += 4;
	}
}

#ifdef CONFIG_MODULES
/*
 * If the exception table is sorted, any referring to the module init
 * will be at the beginning or the end.
 */
void trim_init_extable(struct module *m)
{
	/*trim the beginning*/
	while (m->num_exentries &&
	       within_module_init(ex_insn_addr(&m->extable[0]), m)) {
		m->extable++;
		m->num_exentries--;
	}
	/*trim the end*/
	while (m->num_exentries &&
	       within_module_init(ex_insn_addr(&m->extable[m->num_exentries-1]), m))
		m->num_exentries--;
}
#endif /* CONFIG_MODULES */
