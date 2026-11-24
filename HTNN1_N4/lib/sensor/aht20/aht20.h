#ifndef AHT20_H
#define AHT20_H

#ifdef __cplusplus
extern "C" {
#endif

// Goi sau I2C_Init, chi khoi tao cam bien
void initAHT20(void);

// Moi lan goi thuc hien mot phep do; tra NAN neu chua init hoac doc loi
// Tu thu init lai neu lan init truoc that bai hoac mat ket noi
float readAht20Temp(void); // Nhiet do, do C
float readAht20Humi(void); // Do am, %RH

#ifdef __cplusplus
}
#endif

#endif
