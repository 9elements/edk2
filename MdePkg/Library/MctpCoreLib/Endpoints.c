#include <Base.h>
#include <Uefi.h>

#include <Library/MctpCoreLib.h>

EFIAPI
BOOLEAN
MctpIsAssignableEndpointID (
  IN UINT8  ID
  )
{
  return (ID >= 8 && ID != MCTP_EID_BROADCAST);
}

EFIAPI
BOOLEAN
MctpIsValidEndpointID (
  IN UINT8  ID
  )
{
  return ID == MCTP_EID_NULL ||
         ID == MCTP_EID_BROADCAST ||
         MctpIsAssignableEndpointID (ID);
}

EFIAPI
BOOLEAN
MctpIsTargetEndpointID (
  IN OUT MCTP_BINDING  *Binding,
  IN UINT8             ID
  )
{
  return ID == MCTP_EID_NULL ||
         ID == MCTP_EID_BROADCAST ||
         ID == Binding->OwnEID;
}

EFIAPI
UINT8
MctpGetOwnEndpointID (
  IN OUT MCTP_BINDING  *Binding
  )
{
  return Binding->OwnEID;
}

EFIAPI
VOID
MctpSetOwnEndpointID (
  IN OUT MCTP_BINDING  *Binding,
  IN UINT8             ID
  )
{
  Binding->OwnEID = ID;
}

EFIAPI
VOID
MctpSetBusOwnerEndpointID (
  IN OUT MCTP_BINDING  *Binding,
  IN UINT8             ID
  )
{
  Binding->BusOwnerEID = ID;
}

EFIAPI
UINT8
MctpGetBusOwnerEndpointID (
  IN OUT MCTP_BINDING  *Binding
  )
{
  return Binding->BusOwnerEID;
}

EFIAPI
BOOLEAN
MctpIsBusOwner (
  IN OUT MCTP_BINDING  *Binding
  )
{
  return Binding->IsBusOwner;
}

EFIAPI
VOID
MctpSetBusOwner (
  IN OUT MCTP_BINDING  *Binding,
  BOOLEAN              Value
  )
{
  Binding->IsBusOwner = Value;
}

EFIAPI
BOOLEAN
MctpSupportsStaticEID (
  IN OUT MCTP_BINDING  *Binding
  )
{
  return Binding->SupportsStaticEID;
}

EFIAPI
EFI_STATUS
MctpResetEID (
  IN OUT MCTP_BINDING  *Binding
  )
{
  if (!MctpSupportsStaticEID (Binding)) {
    return EFI_UNSUPPORTED;
  }

  Binding->OwnEID = Binding->StaticEID;
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
MctpSetStaticEID (
  IN OUT MCTP_BINDING  *Binding,
  UINT8                EndpointID
  )
{
  if (!MctpIsValidEndpointID (EndpointID) ||
      !MctpIsAssignableEndpointID (EndpointID))
  {
    return EFI_INVALID_PARAMETER;
  }

  Binding->SupportsStaticEID = TRUE;
  Binding->StaticEID         = EndpointID;
  if (Binding->OwnEID == MCTP_EID_NULL) {
    Binding->OwnEID = EndpointID;
    return EFI_SUCCESS;
  }

  return EFI_ALREADY_STARTED;
}

/**  Returns the static endpoint ID.

  @retval The current static endpoint ID
**/
EFIAPI
UINT8
MctpGetStaticEID (
  IN OUT MCTP_BINDING  *Binding
  )
{
  return Binding->StaticEID;
}
