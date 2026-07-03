#!/usr/bin/python
# -*- coding: utf-8 -*-
##########################################################################
#  2019        Janek Kozicki                                             #
#                                                                        #
#  This program is free software; it is licensed under the terms of the  #
#  GNU General Public License v2 or later. See file LICENSE for details. #
##########################################################################

import subprocess, time, os, sys
import yade
from yade import qt


class TestGUIHelper:
	"""
	This simple class makes screenshots.
	"""

	def __init__(self, name=None, recenter=False, cameraParams=None):
		self.recenter = recenter  # recenter after adjusting camera
		# cameraParams allows to use custom values of (v.lookAt, v.viewDir, v.eyePosition, v.upVector) # ←← in that order.
		# Obtained with:
		#   v=yade.qt.views()[0];
		#   v.lookAt, v.viewDir, v.eyePosition, v.upVector
		# see scripts/checks-and-tests/gui/testGuiMixedAlphaBox.py for example.
		self.cameraParams = cameraParams
		# sometimes the build servers are oveloaded. Let's try 30 seconds and see if it works. When testing locally put here 1 second.
		self.viewWaitTimeSeconds = 30.0
		# previously these two were 0.25 second, but on (sometimes) overloaded build servers that was too fast, and it produced wrong screenshots.
		self.nextScreenshotWaitTime = 3.0
		self.clickWaitTime = 3.0

		self.scrNum = 0
		self.backend = os.environ.get("YADE_TESTGUI_BACKEND", "x11").lower()
		if self.backend not in ("x11", "qt"):
			raise RuntimeError("Unsupported YADE_TESTGUI_BACKEND=" + self.backend)
		# FIXME : this number 14 is hardcoded in scripts/checks-and-tests/gui/testGui.sh when testing if screenshots are present.
		self.maxTestNum = 14
		if (name != None):
			self.name = name
		else:
			self.name = ""

	def getTestNum(self):
		return self.maxTestNum

	def createEmptyFile(self, path):
		with open(path, 'a'):
			os.utime(path, None)

	def finish(self):
		#sys.exit(0)
		os._exit(0)
		#ip = get_ipython()
		#ip.ask_exit()
		#ip.pt_cli.exit()
		#quit()

	def processQtEvents(self):
		app = qt.QApplication.instance()
		if app == None:
			return
		for i in range(0, 5):
			app.processEvents()
			time.sleep(0.05)

	def qtImageFormat(self):
		if hasattr(qt.QImage, "Format_ARGB32"):
			return qt.QImage.Format_ARGB32
		return qt.QImage.Format.Format_ARGB32

	def qtTopLevelWidget(self, title):
		app = qt.QApplication.instance()
		if app == None:
			return None
		for widget in app.topLevelWidgets():
			if str(widget.windowTitle()) == title:
				return widget
		for widget in app.topLevelWidgets():
			if title in str(widget.windowTitle()):
				return widget
		return None

	def drawPlaceholder(self, painter, x, y, width, height, title):
		painter.fillRect(x, y, width, height, qt.QColor(238, 238, 238))
		painter.setPen(qt.QColor(90, 90, 90))
		painter.drawRect(x, y, width - 1, height - 1)
		painter.drawText(x + 12, y + 24, title + " not open")

	def drawWidget(self, painter, widget, x, y, width, height, title):
		if widget == None:
			self.drawPlaceholder(painter, x, y, width, height, title)
			return
		widget.show()
		widget.resize(width, height)
		self.processQtEvents()
		if hasattr(widget, "grabFramebuffer"):
			pixmap = qt.QPixmap.fromImage(widget.grabFramebuffer())
		else:
			pixmap = widget.grab()
		if pixmap.isNull():
			self.drawPlaceholder(painter, x, y, width, height, title)
			return
		painter.drawPixmap(x, y, width, height, pixmap)

	def drawTerminalArea(self, painter):
		painter.fillRect(0, 560, 550, 640, qt.QColor(18, 18, 18))
		painter.setPen(qt.QColor(220, 220, 220))
		painter.setFont(qt.QFont("monospace", 10))
		lines = [
		        "YADE testGui Qt-native backend",
		        "Test: " + self.name,
		        "Stage: " + str(self.scrNum) + "    Iter: " + str(O.iter),
		        "",
		        "Process output is written to screenshots/testGui_" + self.name + ".txt",
		        "This screenshot is composed from Qt widget grabs.",
		]
		for i, line in enumerate(lines):
			painter.drawText(12, 586 + i * 20, line)

	def makeQtScreenshot(self):
		self.processQtEvents()
		image = qt.QImage(1600, 1200, self.qtImageFormat())
		image.fill(qt.QColor(245, 245, 245))
		painter = qt.QPainter(image)
		self.drawTerminalArea(painter)
		self.drawWidget(painter, self.qtTopLevelWidget("Primary view"), 0, 0, 550, 560, "View")
		self.drawWidget(painter, getattr(yade.qt, "controller", None), 550, 20, 500, 1100, "Controller")
		inspector = None
		if getattr(yade.qt, "controller", None) != None:
			inspector = getattr(yade.qt.controller, "inspector", None)
		self.drawWidget(painter, inspector, 1050, 20, 500, 1100, "Inspector")
		painter.end()
		filename = "scr" + "_" + self.name + "_" + str(self.scrNum).zfill(2) + ".png"
		if not image.save(filename):
			raise RuntimeError("Failed to save " + filename)

	def makeNextScreenshot(self):
		time.sleep(self.nextScreenshotWaitTime)
		if self.backend == "x11":
			subprocess.run(["/usr/bin/scrot", "-z", "scr" + "_" + self.name + "_" + str(self.scrNum).zfill(2) + ".png"])
		else:
			self.makeQtScreenshot()
		time.sleep(self.nextScreenshotWaitTime)

	def clickOnScreen(self, x, y, mouseButton=1):
		if self.backend != "x11":
			return
		time.sleep(self.clickWaitTime)
		subprocess.run(["/usr/bin/xdotool", "mousemove", str(x), str(y), "click", str(mouseButton), "mousemove", "restore"])
		time.sleep(self.clickWaitTime)

	def openView(self):
		if self.backend == "x11":
			self.clickOnScreen(634, 1054)
			return
		if (len(yade.qt.views()) == 0):
			button = getattr(yade.qt.controller, "show3dButton", None)
			if button != None:
				button.setChecked(False)
				button.click()
			else:
				yade.qt.View()
		self.processQtEvents()

	def openInspector(self):
		if self.backend == "x11":
			self.clickOnScreen(982, 60)
			return
		if getattr(yade.qt.controller, "inspector", None) == None:
			button = getattr(yade.qt.controller, "inspectButton", None)
			if button != None:
				button.click()
			else:
				yade.qt.Inspector()
		yade.qt.controller.inspector.show()
		self.processQtEvents()

	def selectInspectorTab(self, index, x, y):
		if self.backend == "x11":
			self.clickOnScreen(x, y)
			return
		yade.qt.controller.inspector.tabWidget.setCurrentIndex(index)
		self.processQtEvents()

	def selectControllerTab(self, name, x, y):
		if self.backend == "x11":
			self.clickOnScreen(x, y)
			return
		yade.qt.controller.setTabActive(name)
		self.processQtEvents()

	def selectInteractionFromBodyTab(self):
		if self.backend == "x11":
			self.clickOnScreen(1494, 55)
		else:
			self.processQtEvents()

	def screenshotEngine(self):
		self.scrNum += 1
		intro = "Test '" + self.name + "', stage:" + str(self.scrNum) + " iter:" + str(O.iter)
		if (self.scrNum == 1):
			self.makeNextScreenshot()
			print(intro + " moving yade.qt.Controller()")
			yade.qt.Controller()
			yade.qt.controller.setWindowTitle('GUI test: ' + self.name)
			yade.qt.controller.setGeometry(550, 20, 500, 1100)
			yade.qt.Renderer().blinkHighlight = 'NEVER'
		if (self.scrNum == 2):
			self.makeNextScreenshot()
			print(intro + " opening yade.qt.View()")
			self.openView()
			# Let's wait and see if it worked
			t = 0.0
			while (len(yade.qt.views()) == 0):
				time.sleep(0.5)
				t += 0.5
				if (t > self.viewWaitTimeSeconds):
					self.createEmptyFile("screenshots/yade_qt_View_FAILED_" + self.name)
					print("*** ERROR: failed to open QT View window by clicking ***")
					self.finish()
			# OK, now we have a view to work with.
			vv = yade.qt.views()[0]
			vv.axes = True
			if (self.cameraParams == None):
				vv.lookAt = (7.978, -4.635, 8.221)
				vv.viewDir = (-0.647, 0.441, -0.620)
				vv.eyePosition = (8.626, -5.076, 8.842)
				vv.upVector = (-0.691, 0.000, 0.721)
			else:
				vv.lookAt, vv.viewDir, vv.eyePosition, vv.upVector = self.cameraParams
			if (self.recenter):
				yade.qt.center()
		if (self.scrNum == 3):
			self.makeNextScreenshot()
			print(intro + " opening yade.qt.Inspector() , setting wire=True, setting intrGeom=True")
			self.openInspector()
			success = False
			for attempt in range(0, 30):
				try:
					yade.qt.controller.inspector.setGeometry(1050, 20, 500, 1100)
					success = True
				except:
					time.sleep(1)
			if (not success):
				self.createEmptyFile("screenshots/mouse_click_the_Inspector_open_FAILED_" + self.name)
				print("\n\n*** ERROR: self.clickOnScreen failed to open Inspector window ***\n\n")
				self.makeNextScreenshot()  # a debug screenshot.
				sys.stdout.flush()
				self.finish()
			yade.qt.controller.inspector.show()
			qt.Renderer().wire = True
			qt.Renderer().intrGeom = True
		if (self.scrNum == 4):
			self.makeNextScreenshot()
			print(intro + " changing tab to bodies, setting intrPhys=True, setting bound=True")
			self.selectInspectorTab(1, 1148, 26)
			# Previously these were used. They work, but do not always trigger crashes.
			# yade.qt.controller.inspector.close()
			# yade.qt.controller.inspector.setGeometry(1050,20,500,1100)
			# yade.qt.controller.inspector.tabWidget.setCurrentIndex(2)
			# yade.qt.controller.inspector.show()
			qt.Renderer().intrPhys = True
			qt.Renderer().bound = True
		if (self.scrNum == 5):
			self.makeNextScreenshot()
			print(intro + " clicking on interaction, setting wire=False, setting intrWire=True")
			self.selectInteractionFromBodyTab()
			qt.Renderer().wire = False
			qt.Renderer().intrWire = True
		if (self.scrNum == 6):
			self.makeNextScreenshot()
			print(intro + " changing tab to interactions, setting wire=False, setting intrWire=True")
			self.selectInspectorTab(2, 1234, 26)
			qt.Renderer().wire = False
			qt.Renderer().intrWire = True
		if (self.scrNum == 7):
			self.makeNextScreenshot()
			print(intro + " changing tab to cell, setting intrPhys=False")
			self.selectInspectorTab(3, 1312, 26)
			qt.Renderer().intrPhys = False
		if (self.scrNum == 8):
			self.makeNextScreenshot()
			print(intro + " changing tab to bodies, setting intrWire=False, setting intrGeom=False, setting intrPhys=True")
			self.selectInspectorTab(1, 1148, 26)
			qt.Renderer().intrWire = False
			qt.Renderer().intrGeom = False
			qt.Renderer().intrPhys = True
		if (self.scrNum == 9):
			self.makeNextScreenshot()
			print(intro + " changing tab to display, setting intrAllWire=True")
			self.selectControllerTab('display', 668, 26)
			# yade.qt.controller.setTabActive('display')
			qt.Renderer().intrAllWire = True
		if (self.scrNum == 10):
			self.makeNextScreenshot()
			print(intro + " changing tab to generator, setting intrGeom=True")
			self.selectControllerTab('generator', 744, 26)
			# yade.qt.controller.setTabActive('generator')
			qt.Renderer().intrGeom = True
		if (self.scrNum == 11):
			self.makeNextScreenshot()
			print(intro + " changing tab to python, setting intrAllWire=False")
			self.selectControllerTab('python', 821, 26)
			# yade.qt.controller.setTabActive('python')
			qt.Renderer().intrAllWire = False
		if (self.scrNum == 12):
			self.makeNextScreenshot()
			print(intro + " changing tab to simulation")
			self.selectControllerTab('simulation', 580, 26)
			# yade.qt.controller.setTabActive('simulation')
		if (self.scrNum == 13):
			print(intro + " (testing of matplotlib is skipped for now...)")
			self.makeNextScreenshot()
			# FIXME: I couldn't get matplotlib to draw the plot, while screenshotting is going on.
			# self.makeNextScreenshot();
			# O.pause()
			# print(intro+" opening yade.plot.plot()")
			# plot.liveInterval=0
			# plot.plot(subPlots=False)
			# fig=yade.plot.plot();
			# time.sleep(5)
			# matplotlib.pyplot.draw()
			# time.sleep(5)
			# self.makeNextScreenshot();
			# matplotlib.pyplot.close(fig)
		if (self.scrNum == 14):
			self.makeNextScreenshot()
			self.createEmptyFile("screenshots/testGui_" + self.name + "_OK_or_Skipped.txt")
			print(intro + " exiting\n\n")
			O.pause()
			vv = yade.qt.views()[0]
			vv.close()
			yade.qt.controller.inspector.close()
			yade.qt.controller.close()
			self.finish()
