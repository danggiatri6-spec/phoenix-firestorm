namespace OpenSimNext.GodotViewer.Protocol;

/// <summary>
/// Transport boundary between the Godot runtime and a Grid protocol adapter.
/// This interface intentionally contains no LLUDP or Capabilities implementation yet.
/// </summary>
public interface IGridConnection
{
    GridConnectionState State { get; }

    Task<GridSessionInfo> ConnectAsync(
        GridLoginRequest request,
        CancellationToken cancellationToken = default);

    Task DisconnectAsync(CancellationToken cancellationToken = default);
}

public enum GridConnectionState
{
    Disconnected,
    Connecting,
    Connected,
    Disconnecting,
    Faulted
}

public sealed record GridLoginRequest(
    Uri LoginUri,
    string Username,
    string Password);

public sealed record GridSessionInfo(
    string AgentId,
    string SessionId,
    string SecureSessionId,
    Uri SimulatorEndpoint,
    string SimulatorAddress,
    int SimulatorPort);
