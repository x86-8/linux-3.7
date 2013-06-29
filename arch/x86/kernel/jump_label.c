/*
 * jump label x86 support
 *
 * Copyright (C) 2009 Jason Baron <jbaron@redhat.com>
 *
 */
#include <linux/jump_label.h>
#include <linux/memory.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/jhash.h>
#include <linux/cpu.h>
#include <asm/kprobes.h>
#include <asm/alternative.h>

#ifdef HAVE_JUMP_LABEL

union jump_code_union {
	char code[JUMP_LABEL_NOP_SIZE];
	struct {
		char jump;
		int offset;
	} __attribute__((packed));
};

/* jump_label 엔트리에 type 대로 적용하며, poker함수 적용 */
static void __jump_label_transform(struct jump_entry *entry,
				   enum jump_label_type type,
				   void *(*poker)(void *, const void *, size_t))
{
	union jump_code_union code;

	/* JUMP_LABEL_ENABLE일때, jmp <ADDR>을 집어넣고, 아니면 NOP처리 */
	/* HELPME: NOP처리하면, pipeline 손해를 보는 것 아닌가? */
	if (type == JUMP_LABEL_ENABLE) {
		code.jump = 0xe9;
		code.offset = entry->target -
				(entry->code + JUMP_LABEL_NOP_SIZE);
	} else
		memcpy(&code, ideal_nops[NOP_ATOMIC5], JUMP_LABEL_NOP_SIZE);

	(*poker)((void *)entry->code, &code, JUMP_LABEL_NOP_SIZE);
}

/* jump_label 타입별로 적용. 이 함수는 boot-time이 아닌 runtime시에
 * 호출되기 때문에, cpu 관련 호출이 들어가게 된다. */
void arch_jump_label_transform(struct jump_entry *entry,
			       enum jump_label_type type)
{
	 /* text_poke_smp를 하기전에 호출 필요 */
	get_online_cpus();
	mutex_lock(&text_mutex);
	__jump_label_transform(entry, type, text_poke_smp);
	mutex_unlock(&text_mutex);
	put_online_cpus();
}

/* jump_label 엔트리에 type 대로 적용 */
__init_or_module void arch_jump_label_transform_static(struct jump_entry *entry,
				      enum jump_label_type type)
{
	__jump_label_transform(entry, type, text_poke_early);
}

#endif
