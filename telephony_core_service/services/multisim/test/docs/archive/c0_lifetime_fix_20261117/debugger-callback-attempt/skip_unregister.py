import lldb
def skip(frame, location, internal_dict):
    error = frame.GetThread().ReturnFromFrame(frame, lldb.SBValue())
    print("BYPASS_UNREGISTER:", error.Success())
    return False
