import os
from reportlab.lib.pagesizes import letter
from reportlab.lib import colors
from reportlab.platypus import (
    SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, PageBreak, KeepTogether, HRFlowable
)
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.pdfgen import canvas

class NumberedCanvas(canvas.Canvas):
    def __init__(self, *args, **kwargs):
        super(NumberedCanvas, self).__init__(*args, **kwargs)
        self._saved_page_states = []

    def showPage(self):
        self._saved_page_states.append(dict(self.__dict__))
        self._startPage()

    def save(self):
        num_pages = len(self._saved_page_states)
        for state in self._saved_page_states:
            self.__dict__.update(state)
            self.draw_header_footer(num_pages)
            super(NumberedCanvas, self).showPage()
        super(NumberedCanvas, self).save()

    def draw_header_footer(self, page_count):
        self.saveState()
        self.setFont("Helvetica", 8)
        self.setFillColor(colors.HexColor("#64748b"))
        
        # Header (on pages after the first)
        if self._pageNumber > 1:
            self.drawString(54, 750, "SproutGuard: Edge AI Crop Stress Monitoring — Technical Specification & Execution Guide")
            self.setStrokeColor(colors.HexColor("#e2e8f0"))
            self.setLineWidth(0.5)
            self.line(54, 744, 558, 744)
        
        # Footer
        self.setStrokeColor(colors.HexColor("#e2e8f0"))
        self.setLineWidth(0.5)
        self.line(54, 45, 558, 45)
        self.drawString(54, 34, "NXP FRDM-MCXW71 | Bluetooth Low Energy 5.3 | Capacitor Mobile Client")
        page_str = f"Page {self._pageNumber} of {page_count}"
        self.drawRightString(558, 34, page_str)
        self.restoreState()

def create_guide_pdf(filename):
    doc = SimpleDocTemplate(
        filename,
        pagesize=letter,
        leftMargin=54,
        rightMargin=54,
        topMargin=54,
        bottomMargin=54
    )

    styles = getSampleStyleSheet()

    title_style = ParagraphStyle(
        'DocTitle',
        parent=styles['Normal'],
        fontName='Helvetica-Bold',
        fontSize=20,
        leading=24,
        textColor=colors.HexColor('#0f172a'),
        spaceAfter=4
    )

    subtitle_style = ParagraphStyle(
        'DocSubtitle',
        parent=styles['Normal'],
        fontName='Helvetica',
        fontSize=11,
        leading=15,
        textColor=colors.HexColor('#475569'),
        spaceAfter=12
    )

    badge_style = ParagraphStyle(
        'DocBadge',
        parent=styles['Normal'],
        fontName='Helvetica-Bold',
        fontSize=8.5,
        leading=11,
        textColor=colors.HexColor('#065f46')
    )

    meta_style = ParagraphStyle(
        'DocMeta',
        parent=styles['Normal'],
        fontName='Helvetica',
        fontSize=8.5,
        leading=12,
        textColor=colors.HexColor('#334155')
    )

    h1_style = ParagraphStyle(
        'DocH1',
        parent=styles['Normal'],
        fontName='Helvetica-Bold',
        fontSize=13,
        leading=17,
        textColor=colors.HexColor('#0f172a'),
        spaceBefore=14,
        spaceAfter=6,
        keepWithNext=True
    )

    h2_style = ParagraphStyle(
        'DocH2',
        parent=styles['Normal'],
        fontName='Helvetica-Bold',
        fontSize=10.5,
        leading=14,
        textColor=colors.HexColor('#047857'),
        spaceBefore=10,
        spaceAfter=4,
        keepWithNext=True
    )

    body_style = ParagraphStyle(
        'DocBody',
        parent=styles['Normal'],
        fontName='Helvetica',
        fontSize=9,
        leading=13,
        textColor=colors.HexColor('#1e293b'),
        spaceAfter=6
    )

    bullet_style = ParagraphStyle(
        'DocBullet',
        parent=styles['Normal'],
        fontName='Helvetica',
        fontSize=8.5,
        leading=12,
        textColor=colors.HexColor('#334155'),
        leftIndent=12,
        spaceAfter=3
    )

    code_style = ParagraphStyle(
        'DocCode',
        parent=styles['Normal'],
        fontName='Courier',
        fontSize=7.8,
        leading=10.5,
        textColor=colors.HexColor('#0f172a')
    )

    story = []

    # Title & Badge
    badge_data = [[
        Paragraph("<b>PROJECT SPECIFICATION & ARCHITECTURE GUIDE</b>", badge_style)
    ]]
    badge_table = Table(badge_data, colWidths=[504])
    badge_table.setStyle(TableStyle([
        ('BACKGROUND', (0,0), (-1,-1), colors.HexColor('#ecfdf5')),
        ('BOX', (0,0), (-1,-1), 1, colors.HexColor('#a7f3d0')),
        ('PADDING', (0,0), (-1,-1), 4),
        ('BOTTOMPADDING', (0,0), (-1,-1), 4),
    ]))
    story.append(badge_table)
    story.append(Spacer(1, 8))

    story.append(Paragraph("SproutGuard: Edge AI Crop Stress Monitor", title_style))
    story.append(Paragraph("Complete Technical Execution Lifecycle, Root Cause Analysis & Codebase Reference", subtitle_style))

    # Meta Table
    meta_data = [
        [
            Paragraph("<b>Target Hardware:</b> NXP FRDM-MCXW71 (Cortex-M33 @ 96 MHz)", meta_style),
            Paragraph("<b>Connectivity:</b> Bluetooth Low Energy 5.3 (GATT Server)", meta_style)
        ],
        [
            Paragraph("<b>Mobile Platform:</b> Android Native App (Capacitor + React 18)", meta_style),
            Paragraph("<b>Git Repository:</b> edge_ai_crop_stress_detection (main)", meta_style)
        ]
    ]
    meta_table = Table(meta_data, colWidths=[252, 252])
    meta_table.setStyle(TableStyle([
        ('BACKGROUND', (0,0), (-1,-1), colors.HexColor('#f8fafc')),
        ('BOX', (0,0), (-1,-1), 1, colors.HexColor('#e2e8f0')),
        ('PADDING', (0,0), (-1,-1), 5),
    ]))
    story.append(meta_table)
    story.append(Spacer(1, 10))

    # Section 1
    story.append(Paragraph("1. Project Architecture & Mission", h1_style))
    story.append(Paragraph(
        "<b>SproutGuard</b> is an intelligent edge-computing agritech telemetry platform engineered to compute real-time physiological crop stress directly at the soil level on an ultra-low-power microcontroller. The system eliminates cloud dependence, cellular latency, and ongoing API subscription costs by hosting localized Edge AI inference on the microcontroller and broadcasting real-time biological telemetry via Bluetooth Low Energy (BLE) to a cross-platform mobile application.",
        body_style
    ))

    arch_cards = [
        [
            Paragraph("<b>Sensing Hardware Rig</b><br/>FRDM-MCXW71 reading capacitive soil moisture (HW-103 on LPADC0), ambient light intensity (ALS-PT19 on LPADC0 Ch 2B), and ambient temperature & relative humidity (DHT11 single-wire bus on PTC0 / Arduino D2).", body_style),
            Paragraph("<b>Onboard Edge AI Matrix</b><br/>High-performance C evaluation computing multi-factor agronomic tolerances (Tomato, Basil, Leafy Greens, Succulent) evaluating heat shock, water deficit, and solar radiation within 11.8 ms.", body_style)
        ],
        [
            Paragraph("<b>BLE Telemetry Server</b><br/>Custom Wireless UART GATT service (UUID <code>01ff0100...</code>) broadcasting clean newline-delimited JSON notification packets every 2 seconds via periodic OSA Timer Manager tasks.", body_style),
            Paragraph("<b>Mobile Application</b><br/>Modern React 18 dashboard packaged with Capacitor. Features line-buffered packet reassembly, safe-area layout compliance, radial stress gauges, and real-time biological remediation instructions.", body_style)
        ]
    ]
    arch_table = Table(arch_cards, colWidths=[250, 250])
    arch_table.setStyle(TableStyle([
        ('BACKGROUND', (0,0), (-1,-1), colors.HexColor('#f8fafc')),
        ('BOX', (0,0), (-1,-1), 1, colors.HexColor('#cbd5e1')),
        ('VALIGN', (0,0), (-1,-1), 'TOP'),
        ('PADDING', (0,0), (-1,-1), 7),
    ]))
    story.append(arch_table)
    story.append(Spacer(1, 10))

    # Section 2
    story.append(Paragraph("2. Step-by-Step Execution Lifecycle", h1_style))
    story.append(Paragraph("From physical power-on to live UI rendering, the system executes through six distinct phases:", body_style))

    steps = [
        ("Step 1: MCU Power-On, Peripheral Clock Gating & Pin Muxing",
         "On reset, <code>main()</code> initializes the 96 MHz FRO core clock. Clocks for Port A, Port B, Port C, and GPIO peripherals are unmasked. Cortex-M33 DWT cycle counter (<code>DWT->CYCCNT</code>) is activated to deliver sub-microsecond timing accuracy. LPADC0 is initialized with a 48 MHz FRO clock and 3.3V reference voltage (<code>kLPADC_ReferenceVoltageAlt1</code>). Analog pins (PTD1, PTD2, PTD3, PTA4) are set to <code>kPORT_PinDisabledOrAnalog</code> to disconnect digital input buffers."),
        
        ("Step 2: BLE Host Initialization & Wireless UART Advertisement",
         "The NXP BLE host initializes the controller interface, connection manager, and GATT database. Characteristic <code>01ff0101-ba5e-f4ee-5ca1-eb1e5e4b1ce0</code> is registered with Notify and Read properties, and its CCCD descriptor (<code>0x2902</code>) is exposed with open permissions. Pairing and bonding flags are cleared (<code>gAppUsePairing_d = 0</code>). The radio broadcasts connectable undirected advertising packets tagged with the complete local name <b>'NXP_WU'</b>."),

        ("Step 3: Periodic Sensor Acquisition & Edge AI Inference Tick",
         "An OSA Timer Manager task fires every 2 seconds (<code>CropTelemetryTimerCallback</code>):<br/>"
         "• <b>DHT11 Digital Read:</b> Pulses PTC0 LOW for 20 ms, releases HIGH for 30 &mu;s, switches to input with internal pull-up, and samples 40 bit-pulses with global interrupts disabled. Verifies checksum byte.<br/>"
         "• <b>Soil Moisture LPADC:</b> Samples Channel 5 (PTD1/A0) and Channel 6 (PTD2/A1). Converts raw millivolts into calibrated soil percentage (470 mV dry = 0%, 180 mV wet = 100%).<br/>"
         "• <b>Light Intensity Lux:</b> Samples phototransistor on ADC0 Channel 2 Side B, mapping millivolts to Lux.<br/>"
         "• <b>Edge AI Computation:</b> <code>CalculateCropStressScore()</code> evaluates multidimensional crop tolerance profiles to generate a real-time stress probability score (0.00 to 1.00)."),

        ("Step 4: Safe JSON Compilation & ATT MTU Chunk Dispatch",
         "To bypass embedded <code>newlib-nano</code> floating-point limitations, values are formatted via integer whole and fractional tenths digits into: <code>{\"soil\":%d.%d,\"temp\":%d.%d,\"hum\":%d.%d,\"lux\":%lu,\"stress\":%d.%d}\\r\\n</code>.<br/>"
         "The string is written to the GATT database attribute. If a central is connected, <code>BleApp_SendUartStream()</code> segments the string into ATT MTU-compatible chunks (typically 20 bytes) and transmits them sequentially via <code>GattServer_SendInstantValueNotification()</code>."),

        ("Step 5: Mobile App Discovery, Connection & CCCD Subscription",
         "When the user launches SproutGuard, <code>bleManager.js</code> uses <code>@capacitor-community/bluetooth-le</code> to scan specifically for <code>NXP_WU</code>. The client initiates a direct GATT connection, discovers the Wireless UART service, and automatically writes <code>0x0001</code> to the CCCD descriptor, instructing the MCU to start pushing notifications."),

        ("Step 6: Stream Reassembly, Resilient Parsing & UI Rendering",
         "Incoming packet chunks are accumulated in a FIFO buffer (<code>rxBuffer</code>). When a newline (<code>\\n</code>) is detected, the line is sanitized of broken tokens and parsed via <code>JSON.parse()</code>. A secondary regex parser acts as a fail-safe. Valid values update React state hooks in <code>App.jsx</code>, dynamically populating the Soil, Temp, Humidity, and Light cards, the stress gauge, and the agronomic diagnosis panel.")
    ]

    for title, desc in steps:
        step_content = [
            Paragraph(f"<b>{title}</b>", h2_style),
            Paragraph(desc, body_style)
        ]
        step_table = Table([[step_content]], colWidths=[504])
        step_table.setStyle(TableStyle([
            ('BACKGROUND', (0,0), (-1,-1), colors.HexColor('#f8fafc')),
            ('LINELEFT', (0,0), (-1,-1), 3, colors.HexColor('#10b981')),
            ('BOX', (0,0), (-1,-1), 0.5, colors.HexColor('#e2e8f0')),
            ('PADDING', (0,0), (-1,-1), 5),
        ]))
        story.append(step_table)
        story.append(Spacer(1, 4))

    story.append(PageBreak())

    # Section 3: Problems & Solutions
    story.append(Paragraph("3. Technical Challenges & Engineering Solutions", h1_style))
    story.append(Paragraph("During development and field debugging, several complex bottlenecks were encountered and resolved:", body_style))

    problems = [
        [
            Paragraph("<b>Symptom & Error</b>", h2_style),
            Paragraph("<b>Root Cause Diagnosis</b>", h2_style),
            Paragraph("<b>Permanent Resolution Implemented</b>", h2_style)
        ],
        [
            Paragraph("<b>Android OS Settings Connection Rejection:</b><br/><i>'Failed to connect to NXP_WU. The connection request was rejected.'</i> (HCI 0x113)", body_style),
            Paragraph("Pairing via Android System Bluetooth Settings triggers Classic Audio (A2DP) / HID profile lookups. The board is an unauthenticated BLE GATT server; pairing negotiation failure provoked an immediate L2CAP disconnect.", body_style),
            Paragraph("Disabled pairing/bonding completely in MCU firmware (<code>gAppUsePairing_d = 0</code>, <code>gAppUseBonding_d = 0</code>). Enforced app-exclusive connection routing via Capacitor BLE client with visual user warnings.", body_style)
        ],
        [
            Paragraph("<b>Blank Widgets Lockout:</b><br/>App shows <i>'CONNECTED - LIVE'</i>, but all 5 dashboard cards remain pinned at <code>--</code>.", body_style),
            Paragraph("<b>Newlib-Nano snprintf Bug:</b> In standard GCC ARM Embedded builds, <code>snprintf()</code> omits floating-point <code>%f</code> formatting by default to save flash. The MCU was transmitting <code>{\"soil\":,\"temp\":,\"hum\":,\"lux\":859,\"stress\":}</code>, crashing <code>JSON.parse()</code> with a <code>SyntaxError</code> every cycle.", body_style),
            Paragraph("Refactored MCU telemetry formatter to compute integer whole and fractional decimal digits explicitly (<code>%d.%d</code>). In the mobile frontend, added pre-parse JSON colon sanitization and robust regex fallback matching.", body_style)
        ],
        [
            Paragraph("<b>GATT Collisions & Link Drops:</b><br/>BLE connection drops after 6–10 seconds; log shows <code>[BLE Native] read error</code>.", body_style),
            Paragraph("A client-side 3-second polling interval executed concurrent <code>GattRead</code> operations on the exact characteristic that was simultaneously streaming periodic notifications, provoking GATT queue collisions in the Android stack.", body_style),
            Paragraph("Decommissioned the automatic 3-second background polling timer in <code>bleManager.js</code>. The link now relies exclusively on clean 2-second MCU push notifications, keeping GATT read available solely for manual sync.", body_style)
        ],
        [
            Paragraph("<b>Duplicate Incomplete Packets:</b><br/>Incoming notification stream showed corrupted, interleaved JSON lines arriving out of sequence.", body_style),
            Paragraph("In <code>wireless_uart.c</code>, after chunking the string through <code>GattServer_SendInstantValueNotification()</code>, an extraneous call to <code>GattServer_SendNotification()</code> was executed, inserting a redundant truncated packet into the queue.", body_style),
            Paragraph("Removed the duplicate notification call, ensuring strictly ordered, single-transmission packet delivery across MTU boundaries.", body_style)
        ],
        [
            Paragraph("<b>DHT11 Timing & Hardware Bus Glitches:</b><br/>Zero temperature and humidity readings or intermittent checksum failures.", body_style),
            Paragraph("Microsecond timing errors caused by RTOS interrupts preempting the single-wire timing loop, coupled with floating input pins lacking unmasked digital buffers in the port PIDR register.", body_style),
            Paragraph("Wrapped the 40-bit transmission window in <code>__disable_irq()</code> and <code>__enable_irq()</code>. Explicitly unmasked input buffers via <code>GPIO_PortInputEnable()</code>, enabled internal pull-up resistors, and utilized hardware DWT cycle counters.", body_style)
        ]
    ]

    prob_table = Table(problems, colWidths=[150, 175, 179])
    prob_table.setStyle(TableStyle([
        ('BACKGROUND', (0,0), (-1,0), colors.HexColor('#f1f5f9')),
        ('BOX', (0,0), (-1,-1), 1, colors.HexColor('#cbd5e1')),
        ('INNERGRID', (0,0), (-1,-1), 0.5, colors.HexColor('#e2e8f0')),
        ('VALIGN', (0,0), (-1,-1), 'TOP'),
        ('PADDING', (0,0), (-1,-1), 5),
    ]))
    story.append(prob_table)
    story.append(Spacer(1, 10))

    # Section 4: Key Source Files
    story.append(Paragraph("4. Main Source Files & Functional Reference", h1_style))

    files_info = [
        ("crop-stress-frdm-mcxw71/source/wireless_uart.c",
         "The central firmware orchestration engine. Implements the 2-second telemetry timer (<code>CropTelemetryTimerCallback</code>), coordinates hardware sensor polling across LPADC and DHT11, encodes integer-decimal JSON strings, writes to GATT database attributes, and handles BLE chunked notification dispatch via <code>BleApp_SendUartStream()</code>."),
        
        ("crop-stress-frdm-mcxw71/source/board_adc.c",
         "Hardware abstraction driver for analog crop sensing. Configures LPADC0 with 48 MHz FRO clock, auto-calibration, and 3.3V reference voltage. Implements <code>BOARD_ReadSoilAdcMv()</code>, <code>CalculateSoilMoisturePercent()</code>, and <code>BOARD_ReadAmbientLightLux()</code>."),

        ("crop-stress-frdm-mcxw71/source/dht11.c",
         "High-precision bit-banged digital single-wire driver for the DHT11 sensor on PTC0 (Arduino D2). Uses Cortex-M33 DWT cycle counter for sub-microsecond pulse measurement, wraps the 40-bit read in interrupt disabling, verifies 8-bit checksums, and caches last-valid data."),

        ("crop-stress-frdm-mcxw71/source/crop_stress.c",
         "Onboard Edge AI Inference implementation. Evaluates multi-variable agronomic stress functions against species tolerance profiles (Tomato, Basil, Leafy Greens, Succulent), producing a real-time composite stress probability score within 11.8 ms."),

        ("crop-stress-mobile/src/services/bleManager.js",
         "Unified cross-platform BLE communication service. Bridges Capacitor Native BLE (Android) and Web Bluetooth API. Implements chunked packet buffering, line-based JSON splitting, syntax sanitization, regex fallback parsing, and state event emission."),

        ("crop-stress-mobile/src/App.jsx",
         "Top-level React interface component. Manages connection state machine, registers BLE telemetry listeners, applies resilient partial state updates, calculates dynamic crop health diagnosis, and renders the header, stress gauge, and sensor grid components.")
    ]

    for fpath, fdesc in files_info:
        file_box = [
            Paragraph(f"<b>File:</b> <code>{fpath}</code>", h2_style),
            Paragraph(fdesc, body_style)
        ]
        ftbl = Table([[file_box]], colWidths=[504])
        ftbl.setStyle(TableStyle([
            ('BACKGROUND', (0,0), (-1,-1), colors.HexColor('#f8fafc')),
            ('BOX', (0,0), (-1,-1), 0.5, colors.HexColor('#cbd5e1')),
            ('PADDING', (0,0), (-1,-1), 5),
        ]))
        story.append(ftbl)
        story.append(Spacer(1, 4))

    story.append(Spacer(1, 6))

    # Verification Box
    verif_content = [
        Paragraph("<b>Final Hardware & Mobile Verification:</b>", h2_style),
        Paragraph("The firmware binary was compiled with Ninja and flashed onto the target FRDM-MCXW71 board via LinkServer. The Android application was built via Gradle and deployed to the connected physical device (OPPO Reno15c 5G). Live data verification confirmed continuous real-time streaming: <b>Soil Moisture: 98.6%</b>, <b>Air Temperature: 34.0°C</b>, <b>Humidity: 76.0%</b>, <b>Sunlight: 30 lx</b>, and <b>Stress Score: 0.21</b>.", body_style)
    ]
    verif_table = Table([[verif_content]], colWidths=[504])
    verif_table.setStyle(TableStyle([
        ('BACKGROUND', (0,0), (-1,-1), colors.HexColor('#f0fdf4')),
        ('BOX', (0,0), (-1,-1), 1, colors.HexColor('#86efac')),
        ('PADDING', (0,0), (-1,-1), 6),
    ]))
    story.append(verif_table)

    doc.build(story, canvasmaker=NumberedCanvas)
    print("PDF Generated successfully:", filename)

if __name__ == '__main__':
    out_pdf = os.path.join(r"c:\Users\ADARSH ACHARYA\Desktop\PROJECTS\IOT\edge_ai_crop", "SproutGuard_System_Architecture_and_Execution_Guide.pdf")
    create_guide_pdf(out_pdf)
