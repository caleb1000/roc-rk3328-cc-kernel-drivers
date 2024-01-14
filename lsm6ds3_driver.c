#include <linux/module.h>
#include <linux/init.h>
#include <linux/spi/spi.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include "lsm6ds3_registers.h"

/* Meta Information */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Caleb Steinmetz");
MODULE_DESCRIPTION("A simple driver for the LSM6DS3 IMU via SPI");

struct my_imu {
	struct spi_device *client;
};

static int my_imu_read_acceleration(struct iio_dev * indio_dev, struct iio_chan_spec const * chan, int *val, int *val2, long mask) {
	struct my_imu *imu = iio_priv(indio_dev);
        uint8_t low_byte, high_byte;
        int16_t raw_value;

        //query channel number to see which axis value is being requested
	if(mask == IIO_CHAN_INFO_RAW) {
            if (chan->channel == 0) {
                //X-Axis
                //pr_info("lsm6ds3_iio - Read x axis\n");
                low_byte = spi_w8r8(imu->client, OUTX_L_XL | LSM6DS3_SPI_READ_STROBE_BM);
                high_byte = spi_w8r8(imu->client,  OUTX_H_XL | LSM6DS3_SPI_READ_STROBE_BM);
            } else if (chan->channel == 1) {
                //Y-Axis
                //pr_info("lsm6ds3_iio - Read y axis\n");
	        low_byte =spi_w8r8(imu->client, OUTY_L_XL | LSM6DS3_SPI_READ_STROBE_BM);
	        high_byte = spi_w8r8(imu->client, OUTY_H_XL | LSM6DS3_SPI_READ_STROBE_BM);
            } else if (chan->channel == 2) {
                //Z-Axis
                //pr_info("lsm6ds3_iio - Read z axis\n");
	        low_byte = spi_w8r8(imu->client, OUTZ_L_XL | LSM6DS3_SPI_READ_STROBE_BM);
	        high_byte = spi_w8r8(imu->client, OUTZ_H_XL | LSM6DS3_SPI_READ_STROBE_BM);
            } else {
                return -EINVAL;
            }

            raw_value = (int16_t)((high_byte << 8) | low_byte);
	    *val = (int)raw_value;
	}
        else if(mask == IIO_CHAN_INFO_SCALE)
        {
            *val = 1;
            *val2 = 16384;
            return IIO_VAL_FRACTIONAL;
        }
	else{
            return -EINVAL;
        }
	return IIO_VAL_INT;
}

static const struct iio_chan_spec my_imu_channels[] = {
    {
        .type = IIO_INCLI,     // Channel type is inclinometer/accelerometer
        .indexed = 1,          // Channel is numerically indexed
        .channel = 0,          // Channel number within the sensor device
        .info_mask_separate = BIT(IIO_CHAN_INFO_RAW), // Specify available information (e.g., raw data)
        .info_mask_shared_by_all = BIT(IIO_CHAN_INFO_SCALE), // Specify shared information (e.g., scale)
        .extend_name = "accel_x", // Extend the channel name
        .scan_index = 0,       // Index for scan order
        .scan_type = {
            .sign = 's',        // Sign of the raw data ('s' for signed)
            .realbits = 16,     // Number of bits in the raw data
            .storagebits = 16,  // Number of bits to store the data
            .shift = 0,         // Shift value for data alignment
            .endianness = IIO_LE, // Endianness of the data (little-endian)
        },
    },
    {
        .type = IIO_INCLI,
        .indexed = 1,
        .channel = 1,
        .info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
        .info_mask_shared_by_all = BIT(IIO_CHAN_INFO_SCALE),
        .extend_name = "accel_y",
        .scan_index = 1,
        .scan_type = {
            .sign = 's',
            .realbits = 16,
            .storagebits = 16,
            .shift = 0,
            .endianness = IIO_LE,
        },
    },
    {
        .type = IIO_INCLI,
        .indexed = 1,
        .channel = 2,
        .info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
        .info_mask_shared_by_all = BIT(IIO_CHAN_INFO_SCALE),
        .extend_name = "accel_z",
        .scan_index = 2,
        .scan_type = {
            .sign = 's',
            .realbits = 16,
            .storagebits = 16,
            .shift = 0,
            .endianness = IIO_LE,
        },
    },
};

static const struct iio_info my_imu_info = {
	.read_raw = my_imu_read_acceleration,
};

/* Declate the probe and remove functions */
static int my_imu_probe(struct spi_device *client);
static void my_imu_remove(struct spi_device *client);

static struct of_device_id my_driver_ids[] = {
	{
		.compatible = "lsm6ds3,myimu",
	}, { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, my_driver_ids);

static struct spi_device_id my_imu[] = {
	{"myimu", 0},
	{ },
};
MODULE_DEVICE_TABLE(spi, my_imu);

static struct spi_driver my_driver = {
	.probe = my_imu_probe,
	.remove = my_imu_remove,
	.id_table = my_imu,
	.driver = {
		.name = "myimu",
		.of_match_table = my_driver_ids,
	},
};

/*
 * @brief This function is called on loading the driver
 */
static int my_imu_probe(struct spi_device *client) {
	struct iio_dev *indio_dev;
	struct my_imu *imu;
	int ret;
        u8 buffer[2];

        pr_info("lsm6ds3_iio - Probe: Device ID: %s\n", spi_get_device_id(client)->name);

	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(struct iio_dev));
	if(!indio_dev) {
		pr_err("lsm6ds3_iio - Error! Out of memory\n");
		return -ENOMEM;
	}

	imu = iio_priv(indio_dev);
	imu->client = client;
	indio_dev->name = "myimu";
	indio_dev->info = &my_imu_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = my_imu_channels;
	indio_dev->num_channels = ARRAY_SIZE(my_imu_channels);
        client->max_speed_hz = 10000000;
	ret = spi_setup(client);
	if(ret < 0) {
		pr_err("lsm6ds3_iio - Failed to set up the SPI Bus\n");
		return ret;
	}

	/*--------------Initialize lsm6ds3-------------------------------*/
        //Software reset
        //Set in SPI 4-Wire mode
        //Set in little-endian
        buffer[0] = CTRL3_C | LSM6DS3_SPI_WRITE_STROBE_BM;
        buffer[1] = CTRL3_C_SW_RESET_BM | CTRL3_C_BLE_LE_BM | CTRL3_C_SIM_4_WIRE_BM;
        ret = spi_write(client, buffer, 2);
        if(ret < 0)
        {
            pr_err("lsm6ds3_iio - Failed to wrtie to CTRL3_C");
            return ret;
        }

        //use 208 Hz mode (0101)
	//use +2g mode which is (00) for Scale
	//use 400 Hz filter (00) for filter
        buffer[0] = CTRL1_XL | LSM6DS3_SPI_WRITE_STROBE_BM;
        buffer[1] = CTRL1_XL_208HZ_BM | CTRL1_XL_SCALE_2G_BM | CTRL1_XL_FILTER_400HZ_BM;
        spi_write(imu->client, buffer, 2);
        if(ret < 0)
        {
            pr_err("lsm6ds3_iio - Failed to wrtie to CTRL1_XL");
            return ret;
        }

        //we need to enable the x,y, and z access
	//default (00), Zen, Yen, Xen, Soft_EN, default(00)
	//X, Y, and Z enabled and soft-iron correction turned off
        buffer[0] = CTRL9_XL | LSM6DS3_SPI_WRITE_STROBE_BM;
        buffer[1] = CTRL9_XL_X_EN_BM |CTRL9_XL_Y_EN_BM | CTRL9_XL_Z_EN_BM | CTRL9_XL_SOFT_DIS_BM;
        spi_write(imu->client, buffer, 2);
        if(ret < 0)
        {
            pr_err("lsm6ds3_iio - Failed to wrtie to CTRL9_XL");
            return ret;
        }

        //Read the WHO_AM_I register to ensure proper initialization
        ret = spi_w8r8(imu->client, WHO_AM_I | LSM6DS3_SPI_READ_STROBE_BM);
        if(ret != WHO_AM_I_EXPECTED_VALUE)
        {
            pr_err("lsm6ds3_iio - Failed to read WHO_AM_I register");
            return EIO;
        }

	spi_set_drvdata(client, indio_dev);

	return devm_iio_device_register(&client->dev, indio_dev);
}

/*
 * @brief This function is called on unloading the driver
 */
static void my_imu_remove(struct spi_device *client) {
	pr_info("lsm6ds3_iio - Removing device!\n");
}

/* This will create the init and exit function automatically */
module_spi_driver(my_driver);

