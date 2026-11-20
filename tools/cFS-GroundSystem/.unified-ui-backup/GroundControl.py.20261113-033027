#!/usr/bin/env python3
"""Unified operator panel within NASA cFS-GroundSystem.

Runs its ORIGINAL RoutingService (UDP telemetry -> ZeroMQ) in this process,
subscribes to those messages, and uses NASA cmdUtil for uplink commands.
No separate NASA Qt windows, no web backend, no Raspberry Pi required to open UI.
This is a lab/operator interface, NOT flight-qualified GSE.
"""
from __future__ import annotations

import collections
from datetime import datetime
import getpass
import math
from pathlib import Path
import socket
import sys
import time

from PyQt5.QtCore import Qt, QThread, QTimer, pyqtSignal, QPointF, QRectF
from PyQt5.QtGui import QColor, QPainter, QPen
from PyQt5.QtWidgets import (
    QApplication, QCheckBox, QComboBox, QDoubleSpinBox, QFormLayout,
    QFrame, QGridLayout, QGroupBox, QHBoxLayout, QHeaderView, QLabel,
    QLineEdit, QMainWindow, QMessageBox, QPlainTextEdit, QPushButton,
    QScrollArea, QSpinBox, QTableWidget, QTableWidgetItem,
    QVBoxLayout, QWidget, QAbstractItemView
)
import zmq

import RoutingService as nasa_routing
from unified_logic import (
    APP_COMMANDS, SENSITIVE, build_command_argv, decode_fields,
    get_mid, noise_std, parse_pages, send_command, validate_ip,
)

ROOT = Path(__file__).resolve().parent
TELEMETRY_DIR = ROOT / 'Subsystems' / 'tlmGUI'
CMDUTIL = ROOT / 'Subsystems' / 'cmdUtil' / 'cmdUtil'

CSS = '''
QMainWindow, QWidget#root, QScrollArea#mainScroll { background: #080f1b; }
QWidget { color: #d8e5f7; font-family: "DejaVu Sans", "Ubuntu", sans-serif; font-size: 12px; }
QGroupBox { border: 1px solid #25384c; border-radius: 12px; margin-top: 16px;
            padding: 14px 11px 11px; background: #101d2d; font-weight: 700; }
QGroupBox::title { subcontrol-origin: margin; left: 15px; top: 1px; padding: 0 7px;
                   color: #a7c2e0; }
QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox { background: #091625; border: 1px solid #30455d;
  border-radius: 7px; padding: 7px 8px; min-height: 20px; selection-background-color: #257f91; }
QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus { border-color:#48d6c6; }
QComboBox QAbstractItemView { background: #0c1b2c; selection-background-color: #205269; }
QPushButton { background: #203b53; border: 1px solid #37536b; border-radius: 8px;
              padding: 10px 12px; font-weight: 650; }
QPushButton:hover { background: #2a526a; }
QPushButton:pressed { background: #376981; }
QPushButton:disabled { color:#647d92; background:#182737; border-color:#293d50; }
QPushButton#primary { background:#38cbb6; color:#04121a; border: none; }
QPushButton#primary:hover { background:#67e3d0; }
QPushButton#stop { background:#7d3048; border-color:#ab4b62; color:#fff; }
QPushButton#stop:hover { background:#a74360; }
QTableWidget, QPlainTextEdit { background:#091625; border: 1px solid #2b3b4c;
   border-radius:6px; alternate-background-color:#12243a; gridline-color:#20374c; }
QTableWidget::item { padding:5px; }
QTableWidget::item:selected { background:#23617b; }
QHeaderView::section { background:#183047; color:#b9d8ed; padding:7px;
                       border:none; border-right:1px solid #344b60; }
QLabel#hint { color:#8da8bf; font-size: 11px; }
QLabel#metric { color:#ffffff; font-size: 24px; font-weight: 800; }
QLabel#subtle { color:#86a9c1; font-size: 11px; }
QCheckBox { spacing:8px; }
QCheckBox::indicator { width:17px; height:17px; }
QScrollBar:vertical { background:#0d1a2a; width:10px; }
QScrollBar::handle:vertical { background:#314d67; border-radius:5px; }
'''


def line(text: str = '', object_name: str = '') -> QLabel:
    label = QLabel(text)
    if object_name:
        label.setObjectName(object_name)
    label.setWordWrap(True)
    return label


class ZeroMQSubscriber(QThread):
    packet_received = pyqtSignal(str, bytes)
    failed = pyqtSignal(str)

    def __init__(self):
        super().__init__()
        self.running = True

    def run(self):
        ctx = zmq.Context()
        sock = ctx.socket(zmq.SUB)
        sock.setsockopt_string(zmq.SUBSCRIBE, 'GroundSystem.')
        sock.connect(f'ipc:///tmp/GroundSystem-{getpass.getuser()}')
        poller = zmq.Poller()
        poller.register(sock, zmq.POLLIN)
        try:
            while self.running:
                if not poller.poll(200):
                    continue
                try:
                    topic, data = sock.recv_multipart(flags=zmq.NOBLOCK)
                    self.packet_received.emit(topic.decode('ascii', errors='replace'), data)
                except (zmq.Again, ValueError):
                    continue
        except zmq.ZMQError as exc:
            self.failed.emit(str(exc))
        finally:
            sock.close(linger=0)
            ctx.term()


class ManagedRoutingService(nasa_routing.RoutingService):
    """Use NASA routing/ZeroMQ unchanged; add cooperative shutdown."""
    def __init__(self):
        super().__init__()
        self.running = True
        self.sock.settimeout(0.25)

    def run(self):
        try:
            self.sock.bind(('', nasa_routing.udp_recv_port))
            while self.running:
                try:
                    datagram, host = self.sock.recvfrom(4096)
                except socket.timeout:
                    continue
                except OSError:
                    break
                if len(datagram) < 6:
                    continue
                ip = host[0]
                if ip not in self.ip_addresses_list:
                    alias = ('Spacecraft%d' % len(self.spacecraft_names)).encode()
                    self.ip_addresses_list.append(ip)
                    self.spacecraft_names.append(alias)
                    self.signal_update_ip_list.emit(ip, alias)
                alias = self.spacecraft_names[self.ip_addresses_list.index(ip)]
                self.forwardMessage(datagram, alias)
        except OSError as exc:
            print('RoutingService bind/receive error:', exc)

    def stop(self):
        self.running = False
        self.wait(900)
        self.sock.close()
        self.publisher.close(linger=0)
        self.context.term()


class SimpleTrend(QWidget):
    """Small chart drawn with Qt itself; no matplotlib requirement."""
    def __init__(self):
        super().__init__()
        self.points = collections.deque(maxlen=180)
        self.setMinimumHeight(165)

    def push(self, angle_deg: float, simulated: bool = False):
        if math.isfinite(angle_deg):
            self.points.append((time.monotonic(), angle_deg))
            self.update()

    def paintEvent(self, event):
        qp = QPainter(self)
        qp.setRenderHint(QPainter.Antialiasing, True)
        qp.fillRect(self.rect(), QColor('#091625'))
        w, h = self.width(), self.height()
        rect = QRectF(54, 20, max(1, w - 68), max(1, h - 48))
        qp.setPen(QPen(QColor('#274054'), 1))
        for k in range(5):
            y = rect.top() + rect.height() * k / 4
            qp.drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y))
        qp.setPen(QColor('#8da8bf'))
        if not self.points:
            qp.drawText(rect, Qt.AlignCenter, 'Chưa có góc Z từ telemetry')
            qp.end()
            return
        values = [v for _, v in self.points]
        ymin, ymax = min(values), max(values)
        if ymax - ymin < 2:
            ymin -= 1
            ymax += 1
        pad = (ymax - ymin) * 0.15
        ymin -= pad
        ymax += pad
        tmax = self.points[-1][0]
        tmin = max(self.points[0][0], tmax - 30.0)
        qp.setPen(QColor('#88a5bf'))
        qp.drawText(6, int(rect.top())+8, f'{ymax:.1f}°')
        qp.drawText(6, int(rect.bottom()), f'{ymin:.1f}°')
        qp.drawText(int(rect.right())-28, h-7, '30s')
        qp.setPen(QPen(QColor('#48d6c6'), 2.4))
        previous = None
        for t, v in self.points:
            if t < tmin:
                continue
            x = rect.left() + rect.width() * (t - tmin) / max(0.1, tmax - tmin)
            y = rect.bottom() - rect.height() * (v - ymin) / max(1e-8, ymax - ymin)
            point = QPointF(x, y)
            if previous is not None:
                qp.drawLine(previous, point)
            previous = point
        qp.end()


class OperatorWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle('ADCS / cFS — Unified Ground Control')
        self.resize(1390, 920)
        self.router = None
        self.subscriber = None
        self.pages = parse_pages(TELEMETRY_DIR / 'telemetry-pages.txt')
        self.page_by_mid = {p.mid:p for p in self.pages}
        self.packet_counts = collections.Counter()
        self.latest_packets = {}
        self.latest_fields = {}
        self.latest_times = {}
        self.alias_by_ip = {}
        self.noise_window = collections.deque(maxlen=80)
        self.total = 0
        self.arm_until = 0.0
        self.tlm_started = False
        self.demo_phase = 0.0
        self.mode_demo = False
        self._setup_ui()
        self._create_rows()
        self.update_app_commands()
        self.pulse = QTimer(self)
        self.pulse.timeout.connect(self.on_tick)
        self.pulse.start(450)
        self.demo_timer = QTimer(self)
        self.demo_timer.timeout.connect(self.demo_sample)
        self.demo_timer.setInterval(350)
        self.log('Ứng dụng sẵn sàng. Nhấn "Bắt đầu nhận" để mở UDP telemetry.')
        self.log('SENT_UNCONFIRMED = gửi UDP, không phải ACK từ cFS.')

    def _setup_ui(self):
        root = QWidget(objectName='root')
        self.setCentralWidget(root)
        main = QVBoxLayout(root)
        main.setContentsMargins(16, 12, 16, 14)
        main.setSpacing(10)

        header = QHBoxLayout()
        title_col = QVBoxLayout()
        title = line('ADCS / GROUND CONTROL')
        title.setStyleSheet('font-size: 25px; font-weight: 900; color: #e8faf9;')
        title_col.addWidget(title)
        title_col.addWidget(line('NASA cFS · Command + Telemetry trong một cửa sổ', 'subtle'))
        header.addLayout(title_col, 1)
        self.status = line('● CHƯA NHẬN TELEMETRY')
        self.status.setStyleSheet('color:#f8c17f; font-weight:800; font-size:13px;')
        header.addWidget(self.status, 0, Qt.AlignRight)
        main.addLayout(header)

        scroll = QScrollArea(objectName='mainScroll')
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.NoFrame)
        inner = QWidget(objectName='root')
        self.content = QVBoxLayout(inner)
        self.content.setContentsMargins(2, 2, 3, 18)
        self.content.setSpacing(12)
        scroll.setWidget(inner)
        main.addWidget(scroll, 1)

        # Network / destination card
        network = QGroupBox('01   KẾT NỐI · CẤU HÌNH MẠNG')
        grid = QGridLayout(network)
        self.ip = QLineEdit('127.0.0.1')
        self.ip.setPlaceholderText('IP cFS / Raspberry Pi')
        self.command_port = QSpinBox(); self.command_port.setRange(1, 65535); self.command_port.setValue(1234)
        self.recv_port = QSpinBox(); self.recv_port.setRange(1, 65535); self.recv_port.setValue(2234)
        self.ground_ip = QLineEdit('127.0.0.1')
        self.ground_ip.setPlaceholderText('IP máy trạm, nơi cFS gửi telemetry')
        grid.addWidget(line('IP cFS / Raspberry Pi', 'hint'), 0, 0)
        grid.addWidget(line('UDP COMMAND', 'hint'), 0, 1)
        grid.addWidget(line('UDP TELEMETRY (máy trạm)', 'hint'), 0, 2)
        grid.addWidget(line('IP máy trạm nhận TLM', 'hint'), 0, 3)
        grid.addWidget(self.ip, 1, 0)
        grid.addWidget(self.command_port, 1, 1)
        grid.addWidget(self.recv_port, 1, 2)
        grid.addWidget(self.ground_ip, 1, 3)
        self.listen_btn = QPushButton('▶  Bắt đầu nhận telemetry'); self.listen_btn.setObjectName('primary')
        self.listen_btn.clicked.connect(self.start_telemetry)
        self.enable_btn = QPushButton('↗  Bật TLM trên cFS')
        self.enable_btn.clicked.connect(self.enable_telemetry)
        self.demo_check = QCheckBox('Chế độ DEMO (không gửi lệnh thật)')
        self.demo_check.toggled.connect(self.set_demo)
        grid.addWidget(self.listen_btn, 2, 0, 1, 2)
        grid.addWidget(self.enable_btn, 2, 2)
        grid.addWidget(self.demo_check, 2, 3)
        grid.addWidget(line('Bắt đầu nhận → Bật TLM → Quan sát packet. Chọn IP máy ảo khi chạy cFS trên cùng Ubuntu.', 'hint'), 3, 0, 1, 4)
        self.content.addWidget(network)

        # Quick overview
        overview = QHBoxLayout()
        self.metric_packets = self._metric_card(overview, 'GÓI ĐÃ NHẬN', '0')
        self.metric_angle = self._metric_card(overview, 'GÓC Z · YAW', '—')
        self.metric_rate = self._metric_card(overview, 'TỐC ĐỘ GÓC Z', '—')
        self.metric_wheel = self._metric_card(overview, 'TỐC ĐỘ BÁNH ĐÀ', '—')
        self.content.addLayout(overview)

        # Both command and telemetry always visible side-by-side.
        row = QHBoxLayout()
        row.setSpacing(12)
        commands_card = QGroupBox('02   GỬI LỆNH cFS')
        left = QVBoxLayout(commands_card)
        form = QFormLayout()
        self.app_select = QComboBox()
        self.app_select.addItems(list(APP_COMMANDS))
        self.app_select.setCurrentText('IMU_APP')
        self.app_select.currentTextChanged.connect(self.update_app_commands)
        self.command_select = QComboBox()
        self.command_select.currentTextChanged.connect(self.update_command_fields)
        self.speed = QDoubleSpinBox()
        self.speed.setRange(-60, 60)
        self.speed.setDecimals(2)
        self.speed.setSuffix(' rad/s')
        form.addRow('Ứng dụng', self.app_select)
        form.addRow('Lệnh', self.command_select)
        form.addRow('Tốc độ bánh đà', self.speed)
        left.addLayout(form)
        self.arm_check = QCheckBox('ARM 60 giây để gửi lệnh nhạy cảm')
        self.arm_check.toggled.connect(self.arm_changed)
        left.addWidget(self.arm_check)
        self.arm_info = line('Chưa ARM · NOOP có thể gửi trực tiếp', 'hint')
        left.addWidget(self.arm_info)
        self.send_btn = QPushButton('↗  GỬI LỆNH'); self.send_btn.setObjectName('primary')
        self.send_btn.clicked.connect(self.send_selected)
        left.addWidget(self.send_btn)
        self.stop_btn = QPushButton('■  STOP WHEEL'); self.stop_btn.setObjectName('stop')
        self.stop_btn.clicked.connect(self.send_stop)
        left.addWidget(self.stop_btn)
        left.addWidget(line('Lệnh nhạy cảm cần ARM và dữ liệu thiết bị còn mới. STOP WHEEL cần xác nhận. Chỉ gửi tới IP nội bộ.', 'hint'))
        left.addStretch()
        row.addWidget(commands_card, 1)

        telemetry_card = QGroupBox('03   TELEMETRY · DANH SÁCH PACKET')
        right = QVBoxLayout(telemetry_card)
        self.tlm_table = QTableWidget(0, 4)
        self.tlm_table.setHorizontalHeaderLabels(['Packet', 'MID', 'Số gói', 'Mới nhất'])
        self.tlm_table.setAlternatingRowColors(True)
        self.tlm_table.setEditTriggers(QAbstractItemView.NoEditTriggers)
        self.tlm_table.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.tlm_table.setSelectionMode(QAbstractItemView.SingleSelection)
        self.tlm_table.verticalHeader().setVisible(False)
        self.tlm_table.horizontalHeader().setSectionResizeMode(0, QHeaderView.Stretch)
        self.tlm_table.horizontalHeader().setSectionResizeMode(1, QHeaderView.ResizeToContents)
        self.tlm_table.horizontalHeader().setSectionResizeMode(2, QHeaderView.ResizeToContents)
        self.tlm_table.horizontalHeader().setSectionResizeMode(3, QHeaderView.ResizeToContents)
        self.tlm_table.setMinimumHeight(260)
        self.tlm_table.itemSelectionChanged.connect(self.update_packet_detail)
        right.addWidget(self.tlm_table, 1)
        right.addWidget(line('Chọn một packet để xem các trường đã giải mã. MID chưa định nghĩa vẫn được đếm.', 'hint'))
        row.addWidget(telemetry_card, 2)
        self.content.addLayout(row)

        # ADCS / chart and packet detail
        lower = QHBoxLayout(); lower.setSpacing(12)
        chart_card = QGroupBox('04   ADCS TRỤC Z · NHẬN DỮ LIỆU THẬT')
        chart_v = QVBoxLayout(chart_card)
        self.chart = SimpleTrend()
        chart_v.addWidget(self.chart)
        self.noise_label = line('Độ lệch chuẩn gyro (stationary): chưa đủ mẫu', 'subtle')
        chart_v.addWidget(self.noise_label)
        chart_v.addWidget(line('Chỉ hiển thị góc đo IMU; không có lệnh giữ góc tự động khi chạy LIVE.', 'hint'))
        lower.addWidget(chart_card, 3)
        details_card = QGroupBox('05   CHI TIẾT PACKET')
        dv = QVBoxLayout(details_card)
        self.details = QPlainTextEdit(readOnly=True)
        self.details.setPlaceholderText('Chọn MID trong danh sách telemetry...')
        self.details.setMinimumHeight(180)
        dv.addWidget(self.details)
        self.offset = QSpinBox(); self.offset.setRange(0, 32); self.offset.setValue(0)
        self.offset.setToolTip('Byte cộng thêm vào offsets của telemetry-pages.txt; nguồn hiện tại: 0')
        self.offset.valueChanged.connect(self.update_packet_detail)
        offset_row = QHBoxLayout()
        offset_row.addWidget(line('Telemetry header offset bổ sung', 'hint'))
        offset_row.addWidget(self.offset)
        dv.addLayout(offset_row)
        lower.addWidget(details_card, 2)
        self.content.addLayout(lower)

        logs_card = QGroupBox('06   NHẬT KÝ LỆNH / KẾT NỐI')
        logs_v = QVBoxLayout(logs_card)
        self.logs = QPlainTextEdit(readOnly=True)
        self.logs.setMinimumHeight(110)
        self.logs.document().setMaximumBlockCount(180)
        logs_v.addWidget(self.logs)
        self.content.addWidget(logs_card)

    def _metric_card(self, horizontal, title, initial):
        card = QGroupBox('')
        lay = QVBoxLayout(card)
        lay.addWidget(line(title, 'hint'))
        metric = line(initial, 'metric')
        lay.addWidget(metric)
        horizontal.addWidget(card, 1)
        return metric

    def _create_rows(self):
        self.row_by_mid = {}
        for page in self.pages:
            self._ensure_row(page.mid)
        if self.tlm_table.rowCount():
            self.tlm_table.selectRow(0)

    def _ensure_row(self, mid):
        if mid in self.row_by_mid:
            return self.row_by_mid[mid]
        index = self.tlm_table.rowCount()
        page = self.page_by_mid.get(mid)
        self.tlm_table.insertRow(index)
        for c, value in enumerate((page.name if page else 'Unknown packet',
                                   '0x%04X' % mid, '0', '—')):
            item = QTableWidgetItem(str(value))
            item.setData(Qt.UserRole, mid)
            self.tlm_table.setItem(index, c, item)
        self.row_by_mid[mid] = index
        return index

    def log(self, msg):
        self.logs.appendPlainText(datetime.now().strftime('%H:%M:%S') + '  ' + msg)

    def update_app_commands(self):
        if not hasattr(self, 'command_select'):
            return
        self.command_select.blockSignals(True)
        self.command_select.clear()
        self.command_select.addItems(APP_COMMANDS[self.app_select.currentText()])
        self.command_select.blockSignals(False)
        self.update_command_fields()

    def update_command_fields(self):
        self.speed.setVisible(self.command_select.currentText() == 'SET_WHEEL_SPEED')
        self.speed.setEnabled(not self.mode_demo)

    def arm_changed(self, enabled):
        self.arm_until = time.monotonic() + 60 if enabled else 0.0
        self.arm_info.setText('ARM bật trong tối đa 60 giây' if enabled else 'Chưa ARM')
        if enabled:
            self.log('ARM cho lệnh nhạy cảm: tối đa 60 giây')

    def reset_telemetry_view(self):
        """Never mix DEMO samples with genuine live measurements."""
        self.latest_times.clear()
        self.latest_fields.clear()
        self.latest_packets.clear()
        self.noise_window.clear()
        self.packet_counts.clear()
        self.total = 0
        self.chart.points.clear()
        self.chart.update()
        for i in range(self.tlm_table.rowCount()):
            self.tlm_table.item(i, 2).setText('0')
            self.tlm_table.item(i, 3).setText('—')
        self.refresh_metrics()
        self.update_packet_detail()

    def set_demo(self, enabled):
        self.reset_telemetry_view()
        self.mode_demo = enabled
        self.send_btn.setEnabled(not enabled)
        self.enable_btn.setEnabled(not enabled)
        self.stop_btn.setEnabled(not enabled)
        self.arm_check.setChecked(False)
        self.update_command_fields()
        if enabled:
            self.demo_timer.start()
            self.status.setText('● DEMO / SIMULATION')
            self.status.setStyleSheet('color:#f8c17f; font-weight:800; font-size:13px;')
            self.log('DEMO được bật. Không gửi lệnh thật xuống cFS.')
        else:
            self.demo_timer.stop()
            self.log('DEMO đã tắt. Đợi telemetry thật từ UDP.')
            self.on_tick()

    def demo_sample(self):
        self.demo_phase += 0.035
        angle = 18 * math.sin(self.demo_phase)
        rate = 0.6 * math.cos(self.demo_phase)
        wheel = 85 * math.sin(self.demo_phase * 0.7)
        self._record_fields(0x08D4, {
            'Raw Rate (rad/s)':rate+0.01*math.sin(self.demo_phase*12),
            'Rate (rad/s)':rate,'Angle (rad)': math.radians(angle),
            'Bias (rad/s)': 0.005, 'Valid':1, 'Status':0, 'Stationary':0, 'Simulated':1,
        }, simulated=True)
        self._record_fields(0x08E4, {
            'Cmd Omega (rad/s)':wheel,'Target Omega (rad/s)':wheel,
            'Meas Omega (rad/s)': wheel,'Valid':1,'Status':0,'Simulated':1,
            'Measured':0
        }, simulated=True)
        self.refresh_metrics()

    def start_telemetry(self):
        if self.tlm_started:
            self.log('Đang nhận telemetry. Cần khởi động lại ứng dụng để đổi UDP TLM port.')
            return
        port = self.recv_port.value()
        probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            probe.bind(('0.0.0.0', port))
        except OSError:
            probe.close()
            QMessageBox.warning(self, 'UDP đang được sử dụng',
                                'UDP port %d đang có chương trình khác sử dụng.\n'
                                'Hãy đóng cửa sổ NASA GroundSystem cũ (nếu đang chạy), rồi thử lại.' % port)
            return
        finally:
            probe.close()
        try:
            # Original NASA RoutingService uses a global default. We select the
            # desired port before its worker thread starts; NASA source unchanged.
            nasa_routing.udp_recv_port = port
            self.router = ManagedRoutingService()
            self.router.signal_update_ip_list.connect(self.on_detected_source)
            self.router.start()
            self.subscriber = ZeroMQSubscriber()
            self.subscriber.packet_received.connect(self.on_packet)
            self.subscriber.failed.connect(self.log)
            self.subscriber.start()
            self.tlm_started = True
            self.listen_btn.setEnabled(False)
            self.recv_port.setEnabled(False)
            self.log('Lắng nghe UDP :%d → NASA RoutingService → ZeroMQ → giao diện' % port)
            self.on_tick()
        except Exception as exc:
            self.log('Lỗi bắt đầu nhận telemetry: ' + str(exc))
            QMessageBox.warning(self, 'Lỗi nhận telemetry', str(exc))

    def on_detected_source(self, ip: str, alias_bytes: bytes):
        name = alias_bytes.decode('ascii', errors='replace')
        self.alias_by_ip[ip] = name
        self.log('Phát hiện cFS: %s / %s' % (name, ip))

    def on_packet(self, topic: str, data: bytes):
        if self.mode_demo:
            return
        mid = get_mid(data)
        if mid is None:
            return
        # RoutingService handles host IP discovery. Filter only when a source
        # mapping exists and target IP is not the local default.
        alias = topic.split('.')[1] if '.' in topic else ''
        target_ip = self.ip.text().strip()
        if target_ip not in ('', '127.0.0.1') and target_ip in self.alias_by_ip:
            if self.alias_by_ip[target_ip] != alias:
                return
        definition = self.page_by_mid[mid].definition if mid in self.page_by_mid else ''
        fields = decode_fields(data, TELEMETRY_DIR / definition, self.offset.value()) if definition else {}
        self.latest_packets[mid] = data
        self._record_fields(mid, fields, simulated=False)
        row = self.row_by_mid[mid]
        self.tlm_table.item(row, 3).setText(datetime.now().strftime('%H:%M:%S'))
        self.refresh_metrics()
        self.update_packet_detail()

    def _record_fields(self, mid: int, fields: dict, simulated: bool):
        row = self._ensure_row(mid)
        self.total += 1
        self.packet_counts[mid] += 1
        self.latest_fields[mid] = fields
        self.latest_times[mid] = time.monotonic()
        self.tlm_table.item(row, 2).setText(str(self.packet_counts[mid]))
        self.tlm_table.item(row, 3).setText('DEMO' if simulated else datetime.now().strftime('%H:%M:%S'))
        if mid == 0x08D4:
            rate = fields.get('Rate (rad/s)')
            stationary = fields.get('Stationary', 0)
            if isinstance(rate, (float, int)) and stationary == 1 and fields.get('Valid') == 1 and fields.get('Status') == 0 and fields.get('Simulated', 1) == 0:
                self.noise_window.append(float(rate))
            angle = fields.get('Angle (rad)')
            if isinstance(angle, (float, int)) and math.isfinite(angle):
                self.chart.push(math.degrees(angle), simulated)

    def refresh_metrics(self):
        self.metric_packets.setText(str(self.total))
        imu = self.latest_fields.get(0x08D4, {})
        act = self.latest_fields.get(0x08E4, {})
        angle = imu.get('Angle (rad)')
        rate = imu.get('Rate (rad/s)')
        omega = act.get('Meas Omega (rad/s)')
        self.metric_angle.setText(f'{math.degrees(angle):.2f}°' if isinstance(angle, (int,float)) and math.isfinite(angle) else '—')
        self.metric_rate.setText(f'{math.degrees(rate):.2f}°/s' if isinstance(rate, (int,float)) and math.isfinite(rate) else '—')
        self.metric_wheel.setText(f'{omega:.1f} rad/s' if isinstance(omega, (int,float)) and math.isfinite(omega) else '—')
        std = noise_std(list(self.noise_window))
        self.noise_label.setText('Độ lệch chuẩn gyro khi đứng yên: %.4f °/s (%d mẫu)' %
                                 (math.degrees(std), len(self.noise_window)) if std is not None else
                                 'Nhiễu gyro: cần ≥8 mẫu stationary=1, valid, simulated=0 (đã có %d)' % len(self.noise_window))

    def update_packet_detail(self):
        selected = self.tlm_table.selectedItems()
        if not selected:
            return
        row = selected[0].row()
        item = self.tlm_table.item(row, 1)
        if item is None:
            return
        mid = int(item.text(), 16)
        if mid not in self.latest_fields:
            self.details.setPlainText('0x%04X · Chưa có packet' % mid)
            return
        data = self.latest_packets.get(mid)
        page = self.page_by_mid.get(mid)
        if data is not None and page:
            values = decode_fields(data, TELEMETRY_DIR / page.definition, self.offset.value())
            self.latest_fields[mid] = values
        else:
            values = self.latest_fields[mid]
        lines = ['MID: 0x%04X | %s' % (mid, page.name if page else 'Unknown'),
                 'Nguồn: ' + ('DEMO' if self.mode_demo else 'cFS UDP / NASA RoutingService'),
                 'Count: %d' % self.packet_counts[mid], '']
        if not values:
            lines.append('Không có trường đã giải mã (kiểm tra độ dài packet / offset).')
        else:
            for name, value in values.items():
                lines.append('%-25s %s' % (name + ':', '%0.5f' % value if isinstance(value,float) else value))
        if data is not None:
            lines += ['', 'Raw [%d bytes]:' % len(data), data[:160].hex(' ').upper()]
        self.details.setPlainText('\n'.join(lines))

    def on_tick(self):
        if self.arm_until and time.monotonic() >= self.arm_until:
            self.arm_check.setChecked(False)
        if self.arm_check.isChecked():
            self.arm_info.setText('ARM còn %d giây' % max(0, self.arm_until - time.monotonic()))
        if self.mode_demo:
            return
        if not self.tlm_started:
            msg, color = '● CHƯA BẮT ĐẦU NHẬN', '#f8c17f'
        elif self.latest_times:
            latest = max(self.latest_times.values())
            if time.monotonic() - latest < 5:
                msg, color = '● LIVE · CÓ TELEMETRY', '#48d6c6'
            else:
                msg, color = '● TLM TIMEOUT · KHÔNG CÓ GÓI MỚI', '#f8c17f'
        else:
            msg, color = '● ĐANG LẮNG NGHE · 0 PACKET', '#f8c17f'
        self.status.setText(msg)
        self.status.setStyleSheet('color:%s; font-weight:800; font-size:13px;' % color)

    def _dispatch(self, app: str, name: str):
        if self.mode_demo:
            self.log('DEMO: không gửi lệnh phần cứng')
            return
        ip = self.ip.text().strip()
        try:
            validate_ip(ip)
            speed = float(self.speed.value())
            if name in SENSITIVE:
                if not self.arm_check.isChecked() or time.monotonic() > self.arm_until:
                    raise ValueError('Cần ARM trong 60 giây trước khi gửi lệnh nhạy cảm')
                mid = 0x08D4 if app == 'IMU_APP' else 0x08E4
                values = self.latest_fields.get(mid, {})
                last = self.latest_times.get(mid, 0)
                if time.monotonic() - last > 5:
                    raise ValueError('Thiếu telemetry %s mới (5 giây). Không gửi.' % app)
                if values.get('Valid') != 1 or values.get('Status') != 0 or values.get('Simulated') != 0:
                    raise ValueError('Telemetry không valid, status lỗi hoặc simulated=1. Không gửi.')
            if name != 'NOOP':
                msg = ('Gửi %s / %s tới %s:%d?\n\nĐã gửi UDP không đồng nghĩa cFS thực thi.' %
                       (app, name, ip, self.command_port.value()))
                if name == 'SET_WHEEL_SPEED':
                    msg += '\nGiá trị: %.2f rad/s' % speed
                if QMessageBox.question(self, 'Xác nhận gửi lệnh', msg,
                                        QMessageBox.Yes | QMessageBox.No,
                                        QMessageBox.No) != QMessageBox.Yes:
                    return
            argv = build_command_argv(CMDUTIL, ip, self.command_port.value(),
                                      app, name, speed, self.ground_ip.text().strip())
            status = send_command(argv)
            self.log('%s / %s → %s:%d · %s' % (app, name, ip, self.command_port.value(), status))
            if name in SENSITIVE:
                self.arm_check.setChecked(False)
        except (ValueError, RuntimeError) as exc:
            self.log('LỆNH THẤT BẠI: ' + str(exc))
            QMessageBox.warning(self, 'Không gửi được lệnh', str(exc))

    def send_selected(self):
        self._dispatch(self.app_select.currentText(), self.command_select.currentText())

    def send_stop(self):
        self._dispatch('ACT_APP', 'STOP_WHEEL')

    def enable_telemetry(self):
        self._dispatch('TO_LAB', 'ENABLE_TLM')

    def closeEvent(self, event):
        if self.subscriber:
            self.subscriber.running = False
            self.subscriber.wait(1000)
        if self.router:
            try:
                self.router.stop()
            except (OSError, zmq.ZMQError):
                pass
        event.accept()


def main():
    app = QApplication(sys.argv)
    app.setStyle('Fusion')
    app.setStyleSheet(CSS)
    window = OperatorWindow()
    window.show()
    return app.exec_()


if __name__ == '__main__':
    sys.exit(main())
