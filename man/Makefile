# Makefile for installing Equinox Framework man pages

MANDIR = /usr/share/man
MAN3DIR = $(MANDIR)/man3
MAN7DIR = $(MANDIR)/man7

MAN3_PAGES = application.3 module.3 service_controller.3 http_server.3 http_route.3 framework.3
MAN7_PAGES = equinox.7

.PHONY: install uninstall

install:
	@echo "Installing man pages..."
	@mkdir -p $(MAN3DIR)
	@mkdir -p $(MAN7DIR)
	@for page in $(MAN3_PAGES); do \
		install -m 644 $$page $(MAN3DIR)/$$page; \
		echo "Installed $(MAN3DIR)/$$page"; \
	done
	@for page in $(MAN7_PAGES); do \
		install -m 644 $$page $(MAN7DIR)/$$page; \
		echo "Installed $(MAN7DIR)/$$page"; \
	done
	@mandb -q 2>/dev/null || true
	@echo "Man pages installed successfully"
	@echo "Try: man 3 http_server or man 7 equinox"

uninstall:
	@echo "Uninstalling man pages..."
	@for page in $(MAN3_PAGES); do \
		rm -f $(MAN3DIR)/$$page; \
		echo "Removed $(MAN3DIR)/$$page"; \
	done
	@for page in $(MAN7_PAGES); do \
		rm -f $(MAN7DIR)/$$page; \
		echo "Removed $(MAN7DIR)/$$page"; \
	done
	@mandb -q 2>/dev/null || true
	@echo "Man pages uninstalled successfully"
