#ifndef APP_BLE_H
#define APP_BLE_H

#ifdef __cplusplus
extern "C" {
#endif

void ble_start(void);

/* Parse + validate a FACE_SEQ_CHAR write payload and play it on the
   sequence engine. See app_ble.c for the wire format. */
void ble_handle_face_seq_write(const unsigned char *data, unsigned short len);

#ifdef __cplusplus
}
#endif

#endif
