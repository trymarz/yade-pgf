# encoding: utf-8

import yade.config

QT_API = "PyQt6" if "QT6" in yade.config.features else "PyQt5"

USE_QT_WEB_ENGINE = False
WEB_BACKEND_AVAILABLE = True
WEB_IMPORT_ERROR = None

QtWebKit = None
QtWebKitWidgets = None
QtWebEngineWidgets = None

if QT_API == "PyQt6":
	from PyQt6 import QtCore, QtGui, QtWidgets
	from PyQt6.QtCore import *
	from PyQt6.QtGui import *
	from PyQt6.QtWidgets import *

	class _Qt6Compat:
		_enum_types = (
		        QtCore.Qt.AlignmentFlag,
		        QtCore.Qt.FocusPolicy,
		        QtCore.Qt.KeyboardModifier,
		        QtCore.Qt.MouseButton,
		        QtCore.Qt.WindowModality,
		)
		_legacy_aliases = {
		        "MidButton": QtCore.Qt.MouseButton.MiddleButton,
		}

		def __getattr__(self, name):
			if name in self._legacy_aliases:
				return self._legacy_aliases[name]
			if hasattr(QtCore.Qt, name):
				return getattr(QtCore.Qt, name)
			for enum_type in self._enum_types:
				if hasattr(enum_type, name):
					return getattr(enum_type, name)
			raise AttributeError(name)

	Qt = _Qt6Compat()

	for _cls in (QtWidgets.QDialog, QtWidgets.QMenu):
		if not hasattr(_cls, "exec_") and hasattr(_cls, "exec"):
			setattr(_cls, "exec_", getattr(_cls, "exec"))

	if not hasattr(QtWidgets.QDialogButtonBox, "Ok"):
		class QDialogButtonBox(QtWidgets.QDialogButtonBox):
			Ok = QtWidgets.QDialogButtonBox.StandardButton.Ok
			Cancel = QtWidgets.QDialogButtonBox.StandardButton.Cancel
	else:
		QDialogButtonBox = QtWidgets.QDialogButtonBox

	if not hasattr(QtWidgets.QSizePolicy, "Expanding"):
		class QSizePolicy(QtWidgets.QSizePolicy):
			Expanding = QtWidgets.QSizePolicy.Policy.Expanding
	else:
		QSizePolicy = QtWidgets.QSizePolicy

	def qt_exec(obj, *args):
		if hasattr(obj, "exec"):
			return obj.exec(*args)
		return obj.exec_(*args)

	try:
		from PyQt6 import QtWebEngineWidgets
		USE_QT_WEB_ENGINE = True
	except Exception as _qt_web_import_err:
		WEB_BACKEND_AVAILABLE = False
		WEB_IMPORT_ERROR = _qt_web_import_err
else:
	from PyQt5 import QtCore, QtGui, QtWidgets
	from PyQt5.QtCore import *
	from PyQt5.QtGui import *
	from PyQt5.QtWidgets import *

	try:
		from PyQt5 import QtWebKit, QtWebKitWidgets
	except Exception:
		try:
			from PyQt5 import QtWebEngineWidgets
			USE_QT_WEB_ENGINE = True
		except Exception as _qt_web_import_err:
			WEB_BACKEND_AVAILABLE = False
			WEB_IMPORT_ERROR = _qt_web_import_err

	def qt_exec(obj, *args):
		if hasattr(obj, "exec_"):
			return obj.exec_(*args)
		return obj.exec(*args)

__all__ = [name for name in globals() if not name.startswith("_")]
