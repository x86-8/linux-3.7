#ifndef _ASM_X86_RCU_H
#define _ASM_X86_RCU_H

#ifndef __ASSEMBLY__

#include <linux/rcupdate.h>
#include <asm/ptrace.h>

static inline void exception_enter(struct pt_regs *regs)
{
	 /* CONFIG_RCU_USER_QS가 비활성화라면 없음 */
	rcu_user_exit();
}

static inline void exception_exit(struct pt_regs *regs)
{
	 /* RCU_USER_QS는 userspace를 RCU로서 확장하는 것으로, hack을
	  * 하거나, tickless 시스템을 개발하는게 아니라면, 이 옵션으로
	  * 발생하는 불필요한 오버헤드를 방지하기 위해 비활성화 한다고
	  * 한다 */
#ifdef CONFIG_RCU_USER_QS
	if (user_mode(regs))
		rcu_user_enter();
#endif
}

#else /* __ASSEMBLY__ */

#ifdef CONFIG_RCU_USER_QS
# define SCHEDULE_USER call schedule_user
#else
# define SCHEDULE_USER call schedule
#endif

#endif /* !__ASSEMBLY__ */

#endif
