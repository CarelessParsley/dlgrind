.PHONY: all
all: \
  logs/erik.log \
  logs/amane.log \
  logs/heinwald.log

clean:
	rm logs/*.log

logs/%.log:
	./get-config.py $(*F) | build/bin/dlgrind-opt --verbose | tee $@
