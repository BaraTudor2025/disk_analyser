#ifndef DISK_ANALYZER_H
#define DISK_ANALYZER_H

void proc_add(const char* path, int priority);

void proc_suspend(int id);

void proc_resume(int id);

void proc_remove(int id);

void proc_info(int id);

void proc_print(int id);

void proc_list();

#endif
