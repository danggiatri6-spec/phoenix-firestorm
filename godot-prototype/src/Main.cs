using System;
using Godot;
using OpenSimNext.GodotViewer.Protocol;

namespace OpenSimNext.GodotViewer;

public partial class Main : Control
{
    private const string SecondLifeLoginUri = "https://login.secondlife.com/cgi-bin/login.cgi";

    private IGridConnection? _connection;
    private LineEdit _loginUri = null!;
    private LineEdit _firstName = null!;
    private LineEdit _lastName = null!;
    private LineEdit _password = null!;
    private LineEdit _mfaCode = null!;
    private Label _status = null!;
    private Button _loginButton = null!;
    private Button _logoutButton = null!;

    public override void _Ready()
    {
        _connection = new LibreMetaverseGridConnection();

        var margin = new MarginContainer
        {
            AnchorRight = 1.0f,
            AnchorBottom = 1.0f,
            OffsetLeft = 28,
            OffsetTop = 24,
            OffsetRight = -28,
            OffsetBottom = -24
        };
        AddChild(margin);

        var root = new VBoxContainer
        {
            SizeFlagsHorizontal = Control.SizeFlags.ExpandFill,
            SizeFlagsVertical = Control.SizeFlags.ExpandFill
        };
        margin.AddChild(root);

        var title = new Label
        {
            Text = "OpenSim-next",
            HorizontalAlignment = HorizontalAlignment.Left
        };
        title.AddThemeFontSizeOverride("font_size", 30);
        root.AddChild(title);

        var subtitle = new Label
        {
            Text = "Connect to Second Life or an OpenSimulator Grid",
            AutowrapMode = TextServer.AutowrapMode.Word
        };
        root.AddChild(subtitle);
        root.AddChild(new HSeparator());

        root.AddChild(CreateLabel("Grid login URL"));
        _loginUri = CreateInput("https://login.secondlife.com/cgi-bin/login.cgi");
        _loginUri.Text = SecondLifeLoginUri;
        root.AddChild(_loginUri);

        root.AddChild(CreateLabel("First name"));
        _firstName = CreateInput("SL first name");
        root.AddChild(_firstName);

        root.AddChild(CreateLabel("Last name"));
        _lastName = CreateInput("SL last name (use Resident for single-name accounts)");
        root.AddChild(_lastName);

        root.AddChild(CreateLabel("Password"));
        _password = CreateInput("Password");
        _password.Secret = true;
        root.AddChild(_password);

        root.AddChild(CreateLabel("MFA code (if requested by the Grid)"));
        _mfaCode = CreateInput("Optional authentication code");
        root.AddChild(_mfaCode);

        var buttons = new HBoxContainer();
        root.AddChild(buttons);

        _loginButton = new Button { Text = "Log in", CustomMinimumSize = new Vector2(120, 40) };
        _loginButton.Pressed += OnLoginPressed;
        buttons.AddChild(_loginButton);

        _logoutButton = new Button
        {
            Text = "Log out",
            Disabled = true,
            CustomMinimumSize = new Vector2(120, 40)
        };
        _logoutButton.Pressed += OnLogoutPressed;
        buttons.AddChild(_logoutButton);

        _status = new Label
        {
            Text = "Not connected. The default destination is the Second Life official Grid.",
            AutowrapMode = TextServer.AutowrapMode.Word
        };
        root.AddChild(_status);

        var warning = new Label
        {
            Text = "Credentials are used for this login attempt only. Do not share your password in chat or commit it to source control. This prototype is not yet a complete SL Viewer.",
            AutowrapMode = TextServer.AutowrapMode.Word
        };
        warning.AddThemeColorOverride("font_color", new Color(0.72f, 0.72f, 0.72f));
        root.AddChild(warning);
    }

    private static Label CreateLabel(string text) => new() { Text = text };

    private static LineEdit CreateInput(string placeholder) => new()
    {
        PlaceholderText = placeholder,
        SizeFlagsHorizontal = Control.SizeFlags.ExpandFill,
        CustomMinimumSize = new Vector2(0, 36),
        ClearButtonEnabled = true
    };

    private async void OnLoginPressed()
    {
        if (_connection is null)
            return;

        if (!Uri.TryCreate(_loginUri.Text.Trim(), UriKind.Absolute, out var loginUri) ||
            (loginUri.Scheme != Uri.UriSchemeHttps && loginUri.Scheme != Uri.UriSchemeHttp))
        {
            _status.Text = "Please enter a valid HTTP or HTTPS Grid login URL.";
            return;
        }

        var first = _firstName.Text.Trim();
        var last = _lastName.Text.Trim();
        var password = _password.Text;
        if (string.IsNullOrWhiteSpace(first) || string.IsNullOrWhiteSpace(last) ||
            string.IsNullOrEmpty(password))
        {
            _status.Text = "First name, last name, and password are required.";
            return;
        }

        _loginButton.Disabled = true;
        _status.Text = "Connecting…";
        try
        {
            var request = new GridLoginRequest(
                loginUri,
                first,
                last,
                password,
                "last",
                _mfaCode.Text.Trim());

            var session = await _connection.ConnectAsync(request);
            _password.Clear();
            _mfaCode.Clear();
            _status.Text = $"Connected. Avatar: {session.AgentId}\nRegion: {session.SimulatorName}\nSimulator: {session.SimulatorAddress}:{session.SimulatorPort}";
            _logoutButton.Disabled = false;
        }
        catch (Exception ex)
        {
            _status.Text = $"Login failed: {ex.Message}";
        }
        finally
        {
            _password.Clear();
            _loginButton.Disabled = false;
        }
    }

    private async void OnLogoutPressed()
    {
        if (_connection is null)
            return;

        _logoutButton.Disabled = true;
        _status.Text = "Disconnecting…";
        try
        {
            await _connection.DisconnectAsync();
            _status.Text = "Disconnected.";
        }
        catch (Exception ex)
        {
            _status.Text = $"Logout error: {ex.Message}";
        }
    }

    public override void _ExitTree()
    {
        if (_connection is IDisposable disposable)
            disposable.Dispose();
        _connection = null;
    }
}
