#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>

#include <Base.h>
#include <Uefi.h>
#include <Library/MctpTransportLib.h>
#include <Library/MctpCoreLib.h>
#include <Library/BaseMemoryLib.h>

#include "Requests.h"
#include "Core.h"

static EFI_STATUS TransportSendMessageRetval;
static BOOLEAN TransportSendMessageCalled;
static MCTP_TRANSPORT_HEADER TransportSendHdr;
uint8_t *TransportSendMessageBodyHeader;
size_t TransportSendMessageBodyHeaderLength;
uint8_t *TransportSendMessageBody;
size_t TransportSendMessageBodyLength;

/* STUB of linked libraries */
EFI_STATUS
MctpTransportSendMessage(
  IN MCTP_TRANSPORT_HEADER *Hdr,
  IN UINT8                 *MessageBodyHeader,
  IN UINTN                 MessageBodyHeaderLength,
  IN UINT8                 *MessageBody,
  IN UINTN                  MessageBodyLength,
  IN UINTN                 TimeoutUsec
  )
{
	memcpy(&TransportSendHdr, Hdr, sizeof(TransportSendHdr));
	TransportSendMessageBodyHeader = malloc(MessageBodyHeaderLength);
	if (!TransportSendMessageBodyHeader) {
		return EFI_OUT_OF_RESOURCES;
	}
	memcpy(TransportSendMessageBodyHeader, MessageBodyHeader, MessageBodyHeaderLength);
	TransportSendMessageBodyHeaderLength = MessageBodyHeaderLength;

	TransportSendMessageBody = malloc(MessageBodyLength);
	if (!TransportSendMessageBody) {
		return EFI_OUT_OF_RESOURCES;
	}
	memcpy(TransportSendMessageBody, MessageBody, MessageBodyLength);
	TransportSendMessageBodyLength = MessageBodyLength;

	TransportSendMessageCalled = TRUE;
	return TransportSendMessageRetval;
}

EFI_STATUS
EFIAPI
MctpTransportReceiveMessage(
  OUT  MCTP_MSG              **Msg,
  OUT  UINTN                 *MsgLength,
  IN   UINTN                 TimeoutUsec
  )
{
	return EFI_SUCCESS;
}


UINT8
EFIAPI
MctpPhysicalGetEndpointInformation(
  VOID
  )
{
  return 0;
}

/* UNIT TESTS */

static void test_MctpCoreRegisterMessageClass(void **state) {
	EFI_STATUS Status;
	UINT8 tests[] = {
		MCTP_TYPE_CONTROL_MSG,
		MCTP_TYPE_PLDM_MSG,
		MCTP_TYPE_NCSI_MSG,
		MCTP_TYPE_ETHERNET_MSG,
		MCTP_TYPE_NVME_MSG,
		MCTP_TYPE_SPDM_MSG,
		MCTP_TYPE_PCI_VENDOR,
		MCTP_TYPE_IANA_VENDOR,
	};

	for (size_t i = 0; i < ARRAY_SIZE(tests); i++) {
		Status = MctpCoreRegisterMessageClass(tests[i]);
		assert_int_equal(Status, EFI_SUCCESS);
	}
	Status = MctpCoreRegisterMessageClass(MCTP_TYPE_IANA_VENDOR + 1);
	assert_int_equal(Status, EFI_INVALID_PARAMETER);
}

static void test_MctpSetStaticEID(void **state) {
	EFI_STATUS Status;

	for (size_t i = 0; i < 8; i++) {
		Status = MctpSetStaticEID(i);
		assert_int_equal(Status, EFI_INVALID_PARAMETER);
	}

	Status = MctpSetStaticEID(0xff);
	assert_int_equal(Status, EFI_INVALID_PARAMETER);

	Status = MctpSetStaticEID(8);
	assert_int_equal(Status, EFI_SUCCESS);
	assert_int_equal(MctpGetStaticEID(), 8);

	Status = MctpSetStaticEID(9);
	assert_int_equal(Status, EFI_ALREADY_STARTED);
	assert_int_equal(MctpGetStaticEID(), 9);

	assert_int_equal(MctpGetOwnEndpointID(), 8);
}


static void test_MctpResponseVendorDefinedMessageType(void **state) {
	EFI_STATUS        Status;
	MCTP_CONTROL_MSG  ControlMsg;
	UINTN             Length;
	MCTP_MSG          Response;

	ZeroMem(&ControlMsg, sizeof(ControlMsg));
	ZeroMem(&Response, sizeof(Response));
	Length = 0;

	Status = MctpResponseVendorDefinedMessageType(NULL, &Length, &Response);
	assert_int_equal(Status, EFI_INVALID_PARAMETER);
	Status = MctpResponseVendorDefinedMessageType(&ControlMsg, NULL, &Response);
	assert_int_equal(Status, EFI_INVALID_PARAMETER);
	Status = MctpResponseVendorDefinedMessageType(&ControlMsg, &Length, NULL);
	assert_int_equal(Status, EFI_INVALID_PARAMETER);

	for (size_t i = 0; i < 0xff; i++) {
		ControlMsg.Body.GetVendorDefinedMessageTypeReq.VendorIDSelector = i;
		Status = MctpResponseVendorDefinedMessageType(&ControlMsg, &Length, &Response);
		assert_int_equal(Status, EFI_UNSUPPORTED);
	}
	MctpCoreRegisterMessageClass(MCTP_TYPE_PCI_VENDOR);
	MctpCoreRegisterMessageClass(MCTP_TYPE_IANA_VENDOR);
	MctpCoreRegisterPCIVendor(0xcafe, 0xdead);
	MctpCoreRegisterIANAVendor(0xabcdefab, 0x1234);

	for (size_t i = 0; i < 0xff; i++) {
		ControlMsg.Body.GetVendorDefinedMessageTypeReq.VendorIDSelector = i;
		Status = MctpResponseVendorDefinedMessageType(&ControlMsg, &Length, &Response);
		if (i == 0) {
			assert_int_equal(Status, EFI_SUCCESS);
			assert_int_equal(Response.Body.ControlResponseMsg.Body.GetVendorDefinedMessageTypeResp.VendorIDSelector, 1);
			assert_int_equal(Response.Body.ControlResponseMsg.Body.GetVendorDefinedMessageTypeResp.VendorID.VendorIDFormat, MCTP_VENDOR_ID_FORMAT_PCI);
			assert_int_equal(Response.Body.ControlResponseMsg.Body.GetVendorDefinedMessageTypeResp.VendorID.Pci.PCIVendorID, 0xcafe);
		} else if (i == 1) {
			assert_int_equal(Status, EFI_SUCCESS);
			assert_int_equal(Response.Body.ControlResponseMsg.Body.GetVendorDefinedMessageTypeResp.VendorIDSelector, 0xff);
			assert_int_equal(Response.Body.ControlResponseMsg.Body.GetVendorDefinedMessageTypeResp.VendorID.VendorIDFormat, MCTP_VENDOR_ID_FORMAT_IANA);
			assert_int_equal(Response.Body.ControlResponseMsg.Body.GetVendorDefinedMessageTypeResp.VendorID.Iana.IANAEnterpriseID, 0xabcdefab);
		}
		else
			assert_int_equal(Status, EFI_UNSUPPORTED);
	}

	NumSupportedMessageTypes = 1;
	NumSupportedVendorDefinedMessages = 0;
}

extern EFI_STATUS
EFIAPI
MctpCoreLibConstructor (
  VOID
  );

int main(void) {
	MctpCoreLibConstructor();

	TransportSendMessageRetval = EFI_SUCCESS;

	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_MctpCoreRegisterMessageClass),
		cmocka_unit_test(test_MctpSetStaticEID),
		cmocka_unit_test(test_MctpResponseVendorDefinedMessageType),

	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}