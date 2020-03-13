#ifndef NO_OPTICAL_MONITOR

#define SFP_OPTICS                              0
#define OPTICS_IN_MONITOR               0xFEFE
#define OM_TIME_DEFAULT                 10
#define OM_TIME_SFP_DEFAULT             3
#ifdef SIDEWINDER_BREAKOUT
#define MAX_NUM_SFP_MONITOR             (8 + (12 + 6 + 6) * 4)
#else
#define MAX_NUM_SFP_MONITOR             60
#endif
#define BROCADE_OUI_LOCATION            37  /* Starts at loc 37 (actual is 38) */
#define BROCADE_OUT_BYTE0               0x0
#define BROCADE_OUT_BYTE1               0x5
#define BROCADE_OUT_BYTE2               0x1E

// cover the BXU, BXD - superx don't have media type yet.
#define SFP_LINK_MEDIA_TYPE_VALID       0xDD

/*****************************************************************************/
/* GBIC device register offset                                               */
/*****************************************************************************/
#define GBIC_TYPE_ID       0
#define GBIC_EXT_TYPE_ID   1
#define GBIC_CON_TYPE      2
#define GBIC_TRANSCEIVER   3
#define GBIC_SONET_TYPE    4
#define GBIC_TRANS_TYPE    6
#define GBIC_ENCODING     11
#define GBIC_BR_NORMAL    12
#define GBIC_LEN_KM       14
#define GBIC_LEN_100M     15
#define GBIC_LEN_10M      16
#define GBIC_LEN_10M_62_5 17
#define GBIC_LEN_COPPER   18
#define GBIC_VENDOR_NAME  20
#define GBIC_VENDOR_OUI   37
#define GBIC_PART_NUM     40
#define GBIC_PART_REV     56
#define GBIC_WAVELENGTH   60
#define GBIC_CC_BASE      63
#define GBIC_OPTIONS      64
#define GBIC_BR_MAX       66
#define GBIC_BR_MIN       67
#define GBIC_SERIAL_NUM   68
#define GBIC_DATE_CODE    84
#define GBIC_DIAG_MONITOR_TYPE      92
#define GBIC_DIAG_MONITOR_OPTION        93
#define GBIC_DIAG_MONITOR_COMPLIANCE    94

#define GBIC_GIG_TYPE      6
#define GBIC_COPPER_TYPE   0x8

#define GBIC_DIAG_MONITOR_FDRY      0x80
#define GBIC_DIAG_MONITOR_FDRY_LEN      2

/*****************************************************************************/
/*  Register field size                                                      */
/*****************************************************************************/
#define GBIC_TRANSCEIVER_LEN     8
#define GBIC_VENDOR_NAME_LEN    16
#define GBIC_VENDOR_OUI_LEN      3
#define GBIC_PART_NUM_LEN       16
#define GBIC_PART_REV_LEN        4
#define GBIC_SERIAL_NUM_LEN     16
#define GBIC_WAVELENGTH_LEN      2
#define GBIC_DATE_CODE_LEN       8
#define GBIC_OPTIONS_LEN         2
#define GBIC_SONET_TYPE_LEN      2

enum SFP_MONITOR_DEVICE_TYPE
{
  SFP_MONITOR_DEVICE_TYPE_NOT_PRESENT = 0x0,
  SFP_MONITOR_DEVICE_TYPE_UNKNOWN = 0x1,
  SFP_MONITOR_DEVICE_TYPE_NOT_FOUNDRY_QUALIFIED = 0x2,
  SFP_MONITOR_DEVICE_TYPE_FOUNDRY_QUALIFIED_NOT_MONITORABLE = 0x3,
  SFP_MONITOR_DEVICE_TYPE_FOUNDRY_QUALIFIED_NOT_MONITORABLE_TRAP_SENT = 0x4,
  SFP_MONITOR_DEVICE_TYPE_FOUNDRY_QUALIFIED_MONITORABLE = 0x5
};


/*****************************************************************************/
/*  SFP monitoring                                                           */
/*****************************************************************************/

#define SFP_MONITOR_VENDOR_DATA_LEN1    100
#define SFP_MONITOR_VENDOR_DATA_LEN2    132

#define SFP_MONITOR_THRESHOLD_OFFSET      0
#define SFP_MONITOR_THRESHOLD_LEN      40
#define SFP_MONITOR_FLAG_OFFSET      112
#define SFP_MONITOR_FLAG_LEN           8
#define SFP_MONITOR_ADVALUE_STATUS_OFFSET      96
#define SFP_MONITOR_ADVALUE_STATUS_LEN         16

#define SFP_TEMPERATURE           96
#define SFP_TEMPERATURE_LEN        2
#define SFP_TX_POWER             102
#define SFP_TX_POWER_LEN           2
#define SFP_RX_POWER             104
#define SFP_RX_POWER_LEN           2
#define SFP_TX_BIAS_CURRENT      100
#define SFP_TX_BIAS_CURRENT_LEN    2

typedef struct SFP_MONITOR_THRESHOLDS
{
  signed short temp_high_alarm;
  signed short temp_low_alarm;
  signed short temp_high_warn;
  signed short temp_low_warn;

  unsigned short voltage_high_alarm;
  unsigned short voltage_low_alarm;
  unsigned short voltage_high_warn;
  unsigned short voltage_low_warn;

  unsigned short bias_high_alarm;
  unsigned short bias_low_alarm;
  unsigned short bias_high_warn;
  unsigned short bias_low_warn;

  unsigned short tx_power_high_alarm;
  unsigned short tx_power_low_alarm;
  unsigned short tx_power_high_warn;
  unsigned short tx_power_low_warn;

  unsigned short rx_power_high_alarm;
  unsigned short rx_power_low_alarm;
  unsigned short rx_power_high_warn;
  unsigned short rx_power_low_warn;

} SFP_MONITOR_THRESHOLDS;

typedef struct SFP_MONITOR
{
  unsigned long inited;         /* device can be monitored */
  unsigned long enable0;        /* capability */
  unsigned long enable1;
  unsigned long flag0;
  unsigned long flag1;
  unsigned long enabled;        /* device tx on */
  unsigned long interval;
  unsigned char monitor_type;
  SFP_MONITOR_THRESHOLDS thresholds;
  PORT_ID sfp_port;
  UINT16 Optical_monitor_use;
} SFP_MONITOR;

typedef struct
{
  int temp;
  int temp_prec;
  int rx_power;
  int rx_power_prec;
  int tx_power;
  int tx_power_prec;
  SFP_MONITOR_THRESHOLDS thresholds;
  signed short temp_reg;
  UINT16 rx_power_reg;
  UINT16 tx_power_reg;
  UINT16 tx_bias_current_reg;
}
sfp_port_optic_t;

extern enum SYS_LOG_MSG_TYPE sys_log_msg_type;

typedef struct
{
  int temp;
  int temp_prec;
  int rx_power;
  int rx_power_prec;
  int tx_power;
  int tx_power_prec;
  XFP_MONITOR_THRESHOLDS thresholds;
  signed short temp_reg;
  UINT16 rx_power_reg;
  UINT16 tx_power_reg;
  UINT16 tx_bias_current_reg;
}
port_optic_t;

extern int g_show_bcm_flow;


#define OPTMON_DTRACE(msg) \
	if(debugGlobal.system.optics) {\
		kprintf("TRC(%s:%d) ", __FILE__, __LINE__); \
		kprintf msg; \
		sys_sleep(1); \
	}

#endif /* NO_OPTICAL_MONITOR */
