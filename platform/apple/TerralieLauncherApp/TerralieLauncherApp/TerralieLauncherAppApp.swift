import SwiftUI

@main
struct TerralieLauncherAppApp: App {
    @NSApplicationDelegateAdaptor(LauncherAppDelegate.self) private var appDelegate
    @StateObject private var model = LauncherViewModel()

    var body: some Scene {
        WindowGroup("Terralite Launcher") {
            TERRALITELauncher(model: model)
                .frame(minWidth: 1040, minHeight: 680)
                .toolbar {
                    ToolbarItemGroup(placement: .primaryAction) {
                        Button {
                            model.refreshVersions()
                        } label: {
                            Label("Refresh", systemImage: "arrow.clockwise")
                        }
                        .help("Refresh")
                    }
                }
                .navigationTitle("Terralite Launcher")
        }
        .commands {
            CommandGroup(after: .newItem) {
                Button("Refresh Versions") {
                    model.refreshVersions()
                }
                .keyboardShortcut("r", modifiers: [.command])
                .help("Refresh the list of available versions")
            }
        }

        Settings {
            SettingsView(store: model)
                .frame(minWidth: 520, idealWidth: 560, minHeight: 420, idealHeight: 460)
                .padding()
        }
    }
}
