PLATFORM := xilinx_u50_gen3x16_xdma_5_202210_1
CONFIG := connectivity.cfg

CONFIG_FLAG := $(if $(wildcard $(CONFIG)),--config $(CONFIG),)

XOS := kyber512_enc.xo kyber768_enc.xo kyber1024_enc.xo
XCLBINS := kyber512_enc.xclbin kyber768_enc.xclbin kyber1024_enc.xclbin

all: $(XCLBINS)
xo: $(XOS)

kyber512_enc.xo:
	KYBER_K_ENV=2 vitis_hls -f run_hls.tcl

kyber768_enc.xo:
	KYBER_K_ENV=3 vitis_hls -f run_hls.tcl

kyber1024_enc.xo:
	KYBER_K_ENV=4 vitis_hls -f run_hls.tcl

%.xclbin: %.xo
	v++ -l -t hw --platform $(PLATFORM) $(CONFIG_FLAG) -s $< -o $@

clean:
	rm -rf *.xo *.xclbin *.log _x .Xil *.jou *.log kyber*_enc