
EXPORTS = SDL_sound.x
HEADERS = \
	../../SDL_sound.h

all: $(EXPORTS)

$(EXPORTS): $(HEADERS)
	perl gendef.pl $(HEADERS) >$@ || rm $@

clean:
	rm -f $(EXPORTS)
