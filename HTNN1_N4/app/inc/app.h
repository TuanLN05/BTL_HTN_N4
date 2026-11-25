#ifndef APP_H
#define APP_H

/**
 * @brief Khoi tao IPC va log UART truoc khi tao task.
 * @retval true Khoi tao thanh cong.
 * @retval false Khong tao duoc queue hoac mutex.
 * @pre Da goi Board_Init().
 */
bool App_Init(void);

#endif
