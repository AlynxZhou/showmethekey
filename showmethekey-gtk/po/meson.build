i18n = import('i18n')
# gettext does not support custom find path???
# It read POTFILES and LIGUAS from the path of meson.build,
# but files in POTFILES are relative to source root, why???
i18n.gettext(meson.project_name(), preset: 'glib')
