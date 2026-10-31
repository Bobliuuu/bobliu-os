#pragma once

void shell_init(void);
void shell_on_line(const char* line);
void shell_submit(const char* line);   /* irq-safe: queue a line */
void shell_poll(void);                  /* run a queued line, from kernel context */
void shell_prompt_public(void);