import QtQuick
import "../services" as Services
import "../components" as Components

/// Network speed widget — shows download/upload rates.
Components.BarWidget {
    icon: "net"
    defaultAccent: Services.Colors.networkUnderline
    text: Services.Fmt.fill(instance.format ?? "↓{rx} ↑{tx}", {
        rx: Services.SystemStats.netRx,
        tx: Services.SystemStats.netTx
    })
}
