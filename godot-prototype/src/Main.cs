using Godot;

namespace OpenSimNext.GodotViewer;

public partial class Main : Control
{
    public override void _Ready()
    {
        var root = new VBoxContainer
        {
            AnchorRight = 1.0f,
            AnchorBottom = 1.0f,
            OffsetLeft = 32,
            OffsetTop = 32,
            OffsetRight = -32,
            OffsetBottom = -32,
            Alignment = BoxContainer.AlignmentMode.Begin
        };
        AddChild(root);

        var title = new Label
        {
            Text = "OpenSim-next · Godot Viewer",
            HorizontalAlignment = HorizontalAlignment.Left
        };
        title.AddThemeFontSizeOverride("font_size", 28);
        root.AddChild(title);

        var subtitle = new Label
        {
            Text = "SL / OpenSim compatibility feasibility prototype",
            AutowrapMode = TextServer.AutowrapMode.Word
        };
        root.AddChild(subtitle);

        root.AddChild(new HSeparator());

        var status = new Label
        {
            Text = "Prototype shell only — no Grid connection or SL protocol implementation is present yet.",
            AutowrapMode = TextServer.AutowrapMode.Word
        };
        root.AddChild(status);

        var scope = new Label
        {
            Text = "Next gate: validate .NET protocol-library integration, then prove login against a controlled OpenSimulator test Grid.",
            AutowrapMode = TextServer.AutowrapMode.Word
        };
        root.AddChild(scope);
    }
}
