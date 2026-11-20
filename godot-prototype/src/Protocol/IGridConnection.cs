using System;
using System.Threading;
using System.Threading.Tasks;

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

/// <summary>
/// Credentials are transient inputs. Callers must not persist or log this record.
/// SL and OpenSimulator logins use first/last names and may require an MFA response.
/// </summary>
public sealed record GridLoginRequest(
    Uri LoginUri,
    string FirstName,
    string LastName,
    string Password,
    string StartLocation = "last",
    string MfaToken = "",
    string MfaHash = "");

/// <summary>
/// Non-secret session metadata exposed to the presentation layer.
/// Session tokens and secure session identifiers must remain inside the protocol core.
/// </summary>
public sealed record GridSessionInfo(
    string AgentId,
    string SimulatorName,
    string SimulatorAddress,
    int SimulatorPort);
