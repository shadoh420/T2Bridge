// =============================================================================
// T2Bridge AutoKit v1.0
// =============================================================================
// T1-style automatic repair kit usage for Tribes 2.
// Uses repair kits automatically when your health drops below a threshold.
//
// INSTALLATION:
//   1. Copy version.dll and T2Bridge.dll to your GameData/ folder
//   2. Copy this script to GameData/base/scripts/autoexec/
//   3. Launch the game - autokit activates automatically
//
// COMMANDS:
//   AutoKit_Status()        - Show current health and status
//   AutoKit_Toggle(1)       - Toggle on/off (or use keybind)
//   AutoKit_SetThreshold(N) - Set threshold 1-99% (default: 65%)
//
// KEYBIND:
//   Options > Controls > find "AutoKitBind" and assign a key
//   Or in console: bindCommand(keyboard, "k", make, "AutoKitBind", true);
// =============================================================================

// --- Configuration ---
$AutoKit::Enabled = true;           // Start enabled
$AutoKit::HealthThreshold = 0.65;   // Use kit when health drops below 65%
$AutoKit::Cooldown = 500;           // Minimum ms between kit uses

// --- Internal State (do not modify) ---
$AutoKit::LastUseTime = 0;
$T2Bridge::Health = 1.0;
$T2Bridge::Valid = false;
$T2Bridge::Connected = false;
$T2Bridge::DataFile = "t2bridge_data.txt";

// =============================================================================
// T2Bridge - Read health data from DLL via file
// =============================================================================

function T2Bridge_Poll() {
    %file = new FileObject();
    
    if (%file.openForRead($T2Bridge::DataFile)) {
        $T2Bridge::Connected = true;
        
        while (!%file.isEOF()) {
            %line = %file.readLine();
            
            if (getSubStr(%line, 0, 1) $= "#")
                continue;
            
            %eq = strstr(%line, "=");
            if (%eq != -1) {
                %key = getSubStr(%line, 0, %eq);
                %val = getSubStr(%line, %eq + 1, 100);
                
                if (%key $= "valid")
                    $T2Bridge::Valid = (%val $= "1");
                else if (%key $= "health")
                    $T2Bridge::Health = %val + 0;
            }
        }
        %file.close();
    } else {
        $T2Bridge::Connected = false;
        $T2Bridge::Valid = false;
    }
    
    %file.delete();
    
    // Check if we should use a repair kit
    if ($AutoKit::Enabled && $T2Bridge::Connected && $T2Bridge::Valid)
        AutoKit_TryUse();
    
    // Continue polling
    schedule(100, 0, "T2Bridge_Poll");
}

// =============================================================================
// AutoKit - Automatic repair kit usage
// =============================================================================

function AutoKit_TryUse() {
    %health = $T2Bridge::Health;
    
    // Use kit if health is below threshold but player is still alive
    if (%health < $AutoKit::HealthThreshold && %health > 0.01) {
        %now = getSimTime();
        if ((%now - $AutoKit::LastUseTime) > $AutoKit::Cooldown) {
            use("RepairKit");
            $AutoKit::LastUseTime = %now;
        }
    }
}

function AutoKit_Toggle(%val) {
    if (%val) {
        $AutoKit::Enabled = !$AutoKit::Enabled;
        if ($AutoKit::Enabled)
            bottomPrint("AutoKit ON - " @ mFloor($AutoKit::HealthThreshold * 100) @ "%", 3, 1);
        else
            bottomPrint("AutoKit OFF", 3, 1);
    }
}

function AutoKit_SetThreshold(%pct) {
    if (%pct >= 1 && %pct <= 99) {
        $AutoKit::HealthThreshold = %pct / 100;
        bottomPrint("AutoKit threshold: " @ %pct @ "%", 3, 1);
    }
}

function AutoKit_Status() {
    echo("=== AutoKit Status ===");
    echo("Enabled: " @ $AutoKit::Enabled);
    echo("Threshold: " @ mFloor($AutoKit::HealthThreshold * 100) @ "%");
    echo("T2Bridge Connected: " @ $T2Bridge::Connected);
    echo("T2Bridge Valid: " @ $T2Bridge::Valid);
    echo("Current Health: " @ mFloor($T2Bridge::Health * 100) @ "%");
}

// =============================================================================
// Keybind Support
// =============================================================================

function AutoKitBind(%val) {
    AutoKit_Toggle(%val);
}

// =============================================================================
// Initialize
// =============================================================================

schedule(2000, 0, "T2Bridge_Poll");
