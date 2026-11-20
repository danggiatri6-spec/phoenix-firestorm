using System;
using System.Threading;
using System.Threading.Tasks;
using LibreMetaverse;

namespace OpenSimNext.GodotViewer.Protocol;

/// <summary>
/// First protocol adapter spike backed by LibreMetaverse.
/// It proves the login/session boundary only; it is not a complete SL Viewer.
/// </summary>
public sealed class LibreMetaverseGridConnection : IGridConnection, IDisposable
{
    private readonly SemaphoreSlim _gate = new(1, 1);
    private GridClient? _client;
    private bool _disposed;

    public GridConnectionState State { get; private set; } = GridConnectionState.Disconnected;

    public async Task<GridSessionInfo> ConnectAsync(
        GridLoginRequest request,
        CancellationToken cancellationToken = default)
    {
        ArgumentNullException.ThrowIfNull(request);

        await _gate.WaitAsync(cancellationToken).ConfigureAwait(false);
        GridClient? candidate = null;

        try
        {
            ObjectDisposedException.ThrowIf(_disposed, this);

            if (_client is not null || State == GridConnectionState.Connected)
                throw new InvalidOperationException("A Grid session is already connected.");

            if (!request.LoginUri.IsAbsoluteUri)
                throw new ArgumentException("The Grid login URI must be absolute.", nameof(request));

            if (string.IsNullOrWhiteSpace(request.FirstName) ||
                string.IsNullOrWhiteSpace(request.LastName) ||
                string.IsNullOrEmpty(request.Password))
            {
                throw new ArgumentException("First name, last name, and password are required.", nameof(request));
            }

            State = GridConnectionState.Connecting;
            candidate = new GridClient();

            candidate.Settings.Connection.MfaEnabled =
                !string.IsNullOrEmpty(request.MfaToken) ||
                !string.IsNullOrEmpty(request.MfaHash);

            var login = candidate.Network.DefaultLoginParams(
                request.FirstName,
                request.LastName,
                request.Password,
                "OpenSim-next Godot Viewer",
                "0.1.0");

            login.URI = request.LoginUri.ToString();
            login.Start = request.StartLocation;
            login.Token = request.MfaToken;
            login.MfaHash = request.MfaHash;

            var success = await candidate.Network
                .LoginAsync(login, cancellationToken)
                .ConfigureAwait(false);

            if (!success)
            {
                var message = string.IsNullOrWhiteSpace(candidate.Network.LoginMessage)
                    ? "The Grid rejected the login request."
                    : candidate.Network.LoginMessage;

                throw new InvalidOperationException(message);
            }

            var simulator = candidate.Network.CurrentSim
                ?? throw new InvalidOperationException("Login succeeded but no current simulator was assigned.");

            var session = new GridSessionInfo(
                candidate.Self.AgentID.ToString(),
                simulator.Name,
                simulator.IPEndPoint.Address.ToString(),
                simulator.IPEndPoint.Port);

            _client = candidate;
            candidate = null;
            State = GridConnectionState.Connected;
            return session;
        }
        catch
        {
            State = GridConnectionState.Faulted;
            throw;
        }
        finally
        {
            candidate?.Dispose();
            _gate.Release();
        }
    }

    public async Task DisconnectAsync(CancellationToken cancellationToken = default)
    {
        await _gate.WaitAsync(cancellationToken).ConfigureAwait(false);

        try
        {
            if (_disposed)
                return;

            var client = _client;
            if (client is null)
            {
                State = GridConnectionState.Disconnected;
                return;
            }

            State = GridConnectionState.Disconnecting;
            _client = null;

            try
            {
                if (client.Network.Connected)
                    client.Network.Logout();
            }
            finally
            {
                client.Dispose();
                State = GridConnectionState.Disconnected;
            }
        }
        finally
        {
            _gate.Release();
        }
    }

    public void Dispose()
    {
        if (_disposed)
            return;

        _disposed = true;

        var client = Interlocked.Exchange(ref _client, null);
        if (client is not null)
        {
            try
            {
                if (client.Network.Connected)
                    client.Network.Logout();
            }
            finally
            {
                client.Dispose();
            }
        }

        _gate.Dispose();
    }
}
