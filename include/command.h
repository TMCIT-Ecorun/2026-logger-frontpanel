#ifndef COMMAND_H
#define COMMAND_H

void cmd_start_logging(void);
void cmd_stop_logging(void);
void cmd_set_sample_rate(const char *channel_name, int rate_hz);

#endif /* COMMAND_H */
