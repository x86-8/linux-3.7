#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/bootmem.h>
#include <linux/percpu.h>
#include <linux/kexec.h>
#include <linux/crash_dump.h>
#include <linux/smp.h>
#include <linux/topology.h>
#include <linux/pfn.h>
#include <asm/sections.h>
#include <asm/processor.h>
#include <asm/setup.h>
#include <asm/mpspec.h>
#include <asm/apicdef.h>
#include <asm/highmem.h>
#include <asm/proto.h>
#include <asm/cpumask.h>
#include <asm/cpu.h>
#include <asm/stackprotector.h>

DEFINE_PER_CPU_READ_MOSTLY(int, cpu_number);
EXPORT_PER_CPU_SYMBOL(cpu_number);

#ifdef CONFIG_X86_64
#define BOOT_PERCPU_OFFSET ((unsigned long)__per_cpu_load)
#else
#define BOOT_PERCPU_OFFSET 0
#endif

DEFINE_PER_CPU(unsigned long, this_cpu_off) = BOOT_PERCPU_OFFSET;
EXPORT_PER_CPU_SYMBOL(this_cpu_off);

unsigned long __per_cpu_offset[NR_CPUS] __read_mostly = {
	[0 ... NR_CPUS-1] = BOOT_PERCPU_OFFSET,
};
EXPORT_SYMBOL(__per_cpu_offset);

/*
 * On x86_64 symbols referenced from code should be reachable using
 * 32bit relocations.  Reserve space for static percpu variables in
 * modules so that they are always served from the first chunk which
 * is located at the percpu segment base.  On x86_32, anything can
 * address anywhere.  No need to reserve space in the first chunk.
 */
#ifdef CONFIG_X86_64
#define PERCPU_FIRST_CHUNK_RESERVE	PERCPU_MODULE_RESERVE
#else
#define PERCPU_FIRST_CHUNK_RESERVE	0
#endif

#ifdef CONFIG_X86_32
/**
 * pcpu_need_numa - determine percpu allocation needs to consider NUMA
 *
 * If NUMA is not configured or there is only one NUMA node available,
 * there is no reason to consider NUMA.  This function determines
 * whether percpu allocation should consider NUMA or not.
 *
 * RETURNS:
 * true if NUMA should be considered; otherwise, false.
 */
static bool __init pcpu_need_numa(void)
{
#ifdef CONFIG_NEED_MULTIPLE_NODES
	pg_data_t *last = NULL;
	unsigned int cpu;

	for_each_possible_cpu(cpu) {
		int node = early_cpu_to_node(cpu);

		if (node_online(node) && NODE_DATA(node) &&
		    last && last != NODE_DATA(node))
			return true;

		last = NODE_DATA(node);
	}
#endif
	return false;
}
#endif

/**
 * pcpu_alloc_bootmem - NUMA friendly alloc_bootmem wrapper for percpu
 * @cpu: cpu to allocate for
 * @size: size allocation in bytes
 * @align: alignment
 *
 * Allocate @size bytes aligned at @align for cpu @cpu.  This wrapper
 * does the right thing for NUMA regardless of the current
 * configuration.
 *
 * RETURNS:
 * Pointer to the allocated area on success, NULL on failure.
 */
/* pcpu를 위한 bootmem 할당 */
static void * __init pcpu_alloc_bootmem(unsigned int cpu, unsigned long size,
					unsigned long align)
{
	const unsigned long goal = __pa(MAX_DMA_ADDRESS);
#ifdef CONFIG_NEED_MULTIPLE_NODES
	int node = early_cpu_to_node(cpu);
	void *ptr;
  
  /* node가 활성화안되어 있으면 일반 bootmem 사용. 활성화 되어 있으면
   * 해당 node에 할당 */
	if (!node_online(node) || !NODE_DATA(node)) {
		ptr = __alloc_bootmem_nopanic(size, align, goal);
		pr_info("cpu %d has no node %d or node-local memory\n",
			cpu, node);
		pr_debug("per cpu data for cpu%d %lu bytes at %016lx\n",
			 cpu, size, __pa(ptr));
	} else {
		ptr = __alloc_bootmem_node_nopanic(NODE_DATA(node),
						   size, align, goal);
		pr_debug("per cpu data for cpu%d %lu bytes on node%d at %016lx\n",
			 cpu, size, node, __pa(ptr));
	}
	return ptr;
#else
	return __alloc_bootmem_nopanic(size, align, goal);
#endif
}

/*
 * Helpers for first chunk memory allocation
 */
static void * __init pcpu_fc_alloc(unsigned int cpu, size_t size, size_t align)
{
	return pcpu_alloc_bootmem(cpu, size, align);
}

static void __init pcpu_fc_free(void *ptr, size_t size)
{
	free_bootmem(__pa(ptr), size);
}

/* percpu를 위한 cpu간 distance 반환 */
static int __init pcpu_cpu_distance(unsigned int from, unsigned int to)
{
#ifdef CONFIG_NEED_MULTIPLE_NODES
	if (early_cpu_to_node(from) == early_cpu_to_node(to))
		return LOCAL_DISTANCE;
	else
		return REMOTE_DISTANCE;
#else
	return LOCAL_DISTANCE;
#endif
}

static void __init pcpup_populate_pte(unsigned long addr)
{
	populate_extra_pte(addr);
}

static inline void setup_percpu_segment(int cpu)
{
#ifdef CONFIG_X86_32
	struct desc_struct gdt;

	pack_descriptor(&gdt, per_cpu_offset(cpu), 0xFFFFF,
			0x2 | DESCTYPE_S, 0x8);
	gdt.s = 1;
	write_gdt_entry(get_cpu_gdt_table(cpu),
			GDT_ENTRY_PERCPU, &gdt, DESCTYPE_S);
#endif
}

/* x86은 이 함수를 타게 된다 */
void __init setup_per_cpu_areas(void)
{
	unsigned int cpu;
	unsigned long delta;
	int rc;

	pr_info("NR_CPUS:%d nr_cpumask_bits:%d nr_cpu_ids:%d nr_node_ids:%d\n",
		NR_CPUS, nr_cpumask_bits, nr_cpu_ids, nr_node_ids);

	/*
	 * Allocate percpu area.  Embedding allocator is our favorite;
	 * however, on NUMA configurations, it can result in very
	 * sparse unit mapping and vmalloc area isn't spacious enough
	 * on 32bit.  Use page in that case.
	 */
#ifdef CONFIG_X86_32
	/* 32bit 한정 first chunk 가 auto인데 numa라면, page로 한다. 32bit에서
	 * embed(2mb단위) 방식은 메모리 할당면에서 ᅠᆼ 안좋기 때문 */
	if (pcpu_chosen_fc == PCPU_FC_AUTO && pcpu_need_numa())
		pcpu_chosen_fc = PCPU_FC_PAGE;
#endif
	rc = -EINVAL;
	/* first chunk 방식이 PAGE가 아니면 auto 또는 embed인데, auto 는
	 * embed, page 순으로 시도하게 된다(결국 PCPU_FC_EMBED == PCPU_FC_AUTO) */
	if (pcpu_chosen_fc != PCPU_FC_PAGE) {
		const size_t dyn_size = PERCPU_MODULE_RESERVE +
			PERCPU_DYNAMIC_RESERVE - PERCPU_FIRST_CHUNK_RESERVE; // 8KB + 20KB - 8KB
		size_t atom_size;

		/*
		 * On 64bit, use PMD_SIZE for atom_size so that embedded
		 * percpu areas are aligned to PMD.  This, in the future,
		 * can also allow using PMD mappings in vmalloc area.  Use
		 * PAGE_SIZE on 32bit as vmalloc space is highly contended
		 * and large vmalloc area allocs can easily fail.
		 */
#ifdef CONFIG_X86_64
		/* 64bit 일때, PS bit를 사용 PAGE 단위를 2MB로 할당하여,
		 * vmalloc의 PMD size align 된 연속적인 공간을 얻기 위해서
		 * 인 것으로 보인다. 32bit에서는 2MB 단위로 요청하면, 자꾸 
		 * 실패해서 체념한 듯.. :) */
		atom_size = PMD_SIZE; // 2MB
#else
		atom_size = PAGE_SIZE;
#endif
		/* embed 방식으로 첫번재 청크를 할당한다. */
		rc = pcpu_embed_first_chunk(PERCPU_FIRST_CHUNK_RESERVE, // 8 << 10
					    dyn_size, atom_size,       // 20KB, 2MB
					    pcpu_cpu_distance,         // func
					    pcpu_fc_alloc, pcpu_fc_free);  // func, func
		if (rc < 0)
			pr_warning("%s allocator failed (%d), falling back to page size\n",
				   pcpu_fc_names[pcpu_chosen_fc], rc);
	}
	if (rc < 0)
		/* `embed`방식으로 첫번재 청크를 할당이 실패하면 `page`방식으로 할당 한다. */
		rc = pcpu_page_first_chunk(PERCPU_FIRST_CHUNK_RESERVE,
					   pcpu_fc_alloc, pcpu_fc_free,
					   pcpup_populate_pte);
	if (rc < 0)
		panic("cannot initialize percpu area (err=%d)", rc);

	/* alrighty, percpu areas up and running */
	delta = (unsigned long)pcpu_base_addr - (unsigned long)__per_cpu_start;
	for_each_possible_cpu(cpu) {
    /*
     * per_cpu_offset()은 percpu variable에 더해져야만 하는 offset이다. 
     * 목적은 certain processor 까지의 거리를 위하여 존재.
     * 대부분의 아키텍쳐는 __per_cpu_offset array를 사용하지만 x86_64는 자신만의 방법이 존재
     */

    /* fc를 초기화 할 때 얻었던, unit offset에 차이값을 더해, 각각 cpu 오프셋을 구해준다 */
		per_cpu_offset(cpu) = delta + pcpu_unit_offsets[cpu];
    /*this_cpu_off라는 포인터에다가 offset저장*/
		per_cpu(this_cpu_off, cpu) = per_cpu_offset(cpu);
    /*cpu number도 함께 저장해준다.*/
		per_cpu(cpu_number, cpu) = cpu;
    
    /*
     * x86_64에서는 percpu_segment와 canary를 사용하지 않는다.
     * canary에 대한 설명은 http://studyfoss.egloos.com/5279959
     * 에서 찾아볼 수 있도록 한다. 
     */
		setup_percpu_segment(cpu);
		setup_stack_canary_segment(cpu);
		/*
		 * Copy data used in early init routines from the
		 * initial arrays to the per cpu data areas.  These
		 * arrays then become expendable and the *_early_ptr's
		 * are zeroed indicating that the static arrays are
		 * gone.
		 */
#ifdef CONFIG_X86_LOCAL_APIC
    /* 기존에 구했던(early) apicid를 pcpu로 이동. */
		per_cpu(x86_cpu_to_apicid, cpu) =
			early_per_cpu_map(x86_cpu_to_apicid, cpu);
		per_cpu(x86_bios_cpu_apicid, cpu) =
			early_per_cpu_map(x86_bios_cpu_apicid, cpu);
#endif
#ifdef CONFIG_X86_32
		per_cpu(x86_cpu_to_logical_apicid, cpu) =
			early_per_cpu_map(x86_cpu_to_logical_apicid, cpu);
#endif
#ifdef CONFIG_X86_64
    /* 각각의 cpu에 irq stack pointer지정. gs+canary영역이 48
     * byte인데, irq_stack을 보호하기 위해 18 byte만큼을 더 둔 것으로
     * 보임(정확하지 않음) */
		per_cpu(irq_stack_ptr, cpu) =
			per_cpu(irq_stack_union.irq_stack, cpu) +
			IRQ_STACK_SIZE - 64;
#endif
#ifdef CONFIG_NUMA
    /* 기존에 구했던(early) NUMA 정보 역시 pcpu로 이동. */
		per_cpu(x86_cpu_to_node_map, cpu) =
			early_per_cpu_map(x86_cpu_to_node_map, cpu);
		/*
		 * Ensure that the boot cpu numa_node is correct when the boot
		 * cpu is on a node that doesn't have memory installed.
		 * Also cpu_up() will call cpu_to_node() for APs when
		 * MEMORY_HOTPLUG is defined, before per_cpu(numa_node) is set
		 * up later with c_init aka intel_init/amd_init.
		 * So set them all (boot cpu and all APs).
		 */
		set_cpu_numa_node(cpu, early_cpu_to_node(cpu));
#endif
		/*
		 * Up to this point, the boot CPU has been using .init.data
		 * area.  Reload any changed state for the boot CPU.
		 */
		if (!cpu)
			switch_to_new_gdt(cpu);
	}

	/* indicate the early static arrays will soon be gone */
#ifdef CONFIG_X86_LOCAL_APIC
	early_per_cpu_ptr(x86_cpu_to_apicid) = NULL;
	early_per_cpu_ptr(x86_bios_cpu_apicid) = NULL;
#endif
#ifdef CONFIG_X86_32
	early_per_cpu_ptr(x86_cpu_to_logical_apicid) = NULL;
#endif
#ifdef CONFIG_NUMA
	early_per_cpu_ptr(x86_cpu_to_node_map) = NULL;
#endif

	/* Setup node to cpumask map */
	setup_node_to_cpumask_map();

	/* Setup cpu initialized, callin, callout masks */
	setup_cpu_local_masks();
}
