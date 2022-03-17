
#include <Base.h>
#include <Uefi.h>

#include <Library/MctpCoreLib.h>

STATIC UINT8 OwnEID = MCTP_EID_NULL;
STATIC UINT8 BusOwnerEID = MCTP_EID_NULL;
STATIC BOOLEAN IsBusOwner;
STATIC BOOLEAN SupportsStaticEID;
STATIC UINT8 StaticEID;

EFIAPI
BOOLEAN
MctpIsAssignableEndpointID(
  IN UINT8 ID
  )
{
  return (ID >= 8 && ID != MCTP_EID_BROADCAST);
}

EFIAPI
BOOLEAN
MctpIsValidEndpointID(
  IN UINT8 ID
  )
{
  return ID == MCTP_EID_NULL ||
         ID == MCTP_EID_BROADCAST ||
         MctpIsAssignableEndpointID(ID);
}

EFIAPI
BOOLEAN
MctpIsTargetEndpointID(
  IN UINT8 ID
  )
{
  return ID == MCTP_EID_NULL ||
         ID == MCTP_EID_BROADCAST ||
         ID == OwnEID;
}

EFIAPI
UINT8
MctpGetOwnEndpointID(
  VOID
  )
{
  return OwnEID;
}

EFIAPI
VOID
MctpSetOwnEndpointID(
  IN UINT8 ID
  )
{
  OwnEID = ID;
}

EFIAPI
VOID
MctpSetBusOwnerEndpointID(
  IN UINT8 ID
  )
{
  BusOwnerEID = ID;
}

EFIAPI
UINT8
MctpGetBusOwnerEndpointID(
  VOID
  )
{
  return BusOwnerEID;
}

EFIAPI
BOOLEAN
MctpIsBusOwner(
  VOID
  )
{
  return IsBusOwner;
}

EFIAPI
VOID
MctpSetBusOwner(
  BOOLEAN Value
  )
{
  IsBusOwner = Value;
}

EFIAPI
BOOLEAN
MctpSupportsStaticEID(
  VOID
  )
{
  return SupportsStaticEID;
}

EFIAPI
EFI_STATUS
MctpResetEID(
  VOID
  )
{
  if (!MctpSupportsStaticEID()) {
    return EFI_UNSUPPORTED;
  }

  OwnEID = StaticEID;
  return EFI_SUCCESS;
}

/**  Assign a static endpoint ID to this node. If the current endpoint ID hasn't been
     assigned by the bus master yet, the current endpoint ID will be updated to the
     static endpoint ID.

  @param  [in]  EndpointID    The static endpoint ID to use.

  @retval EFI_SUCCESS            The command is executed successfully.
  @retval EFI_INVALID_PARAMETER  The given endpoint ID was invalid.
  @retval EFI_ALREADY_STARTED    A valid endpoint ID already had been assigned.
**/
EFIAPI
EFI_STATUS
MctpSetStaticEID(
  UINT8 EndpointID
  )
{
  if (!MctpIsValidEndpointID(EndpointID) ||
      !MctpIsAssignableEndpointID(EndpointID)) {
    return EFI_INVALID_PARAMETER;
  }

  SupportsStaticEID = TRUE;
  StaticEID = EndpointID;
  if (OwnEID == MCTP_EID_NULL) {
    OwnEID = EndpointID;
    return EFI_SUCCESS;
  }
  return EFI_ALREADY_STARTED;
}

/**  Returns the static endpoint ID.

  @retval The current static endpoint ID
**/
EFIAPI
UINT8
MctpGetStaticEID(
  VOID
  )
{
  return StaticEID;
}