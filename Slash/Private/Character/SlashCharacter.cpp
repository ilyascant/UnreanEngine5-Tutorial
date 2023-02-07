#include "Character/SlashCharacter.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GroomComponent.h"
#include "Animation/AnimMontage.h"
#include "Items/Item.h"
#include "Items/Weapons/Weapon.h"
#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "Slash/DebugMacros.h"


ASlashCharacter::ASlashCharacter() {

	PrimaryActorTick.bCanEverTick = true;

	TObjectPtr<UCharacterMovementComponent> CharMov = GetCharacterMovement();
	CharMov->bOrientRotationToMovement = true;
	CharMov->RotationRate = FRotator(0.f, 360.f, 0.f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(GetRootComponent());

	ViewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ViewCamera"));
	ViewCamera->SetupAttachment(SpringArm);

	Hair = CreateDefaultSubobject<UGroomComponent>(TEXT("Hair"));
	Hair->SetupAttachment(GetMesh());
	Hair->AttachmentName = FString("head");

	Eyebrows = CreateDefaultSubobject<UGroomComponent>(TEXT("Eyebrows"));
	Eyebrows->SetupAttachment(GetMesh());
	Eyebrows->AttachmentName = FString("head");

	OverheadWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("OverheadWidget"));
	OverheadWidget->SetupAttachment(RootComponent);

}

void ASlashCharacter::BeginPlay()
{
	Super::BeginPlay();

	ZoomFactor = 0;

	if (TObjectPtr<APlayerController> PC = Cast<APlayerController>(GetController())) {
		if (TObjectPtr<UEnhancedInputLocalPlayerSubsystem> Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
			Subsystem->AddMappingContext(SlashCharacterMappingContext, 0);
	}
	
}

/**
*	CHARACTER MOVEMENT
*/

void ASlashCharacter::Move(const FInputActionValue& Value)
{
	
	const FVector2D MovementVector = Value.Get<FVector2D>();

	if(Controller && MovementVector.IsZero() != true){
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);

		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(ForwardDirection, MovementVector.Y);

		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
	
}

void ASlashCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D LookAxisValue = Value.Get<FVector2D>();

	if (Controller && LookAxisValue.IsZero() != true) {
		AddControllerYawInput(LookAxisValue.X);
		AddControllerPitchInput(LookAxisValue.Y);

	}
}

void ASlashCharacter::Zoom(const FInputActionValue& Value)
{
	const float FloatValue = Value.Get<float>();
	ZoomFactor += FloatValue * 4;
	ZoomFactor = FMath::Clamp<float>(ZoomFactor, 0.f, 1.0f);

	if (Controller)
		ViewCamera->SetFieldOfView(FMath::Lerp(90.f, 60.f, ZoomFactor));

}

/**
*	CHARACTER INTERACTION
*/

void ASlashCharacter::Equip(const FInputActionValue& Value)
{
	TObjectPtr<AWeapon> OverlappingWeapon = Cast<AWeapon>(OverlappingItem);
	if (EquippedWeapon && OverlappingWeapon) {
		EquippedWeapon->Drop(this->GetActorLocation(), this->GetActorForwardVector());
		EquippedWeapon = nullptr;
	};

	const float bValue = Value.Get<bool>();
	
	if(Controller && bValue){
		if (OverlappingWeapon){
			OverlappingWeapon->Equip(GetMesh(), FName("RightHandSocket"));
			OverlappingWeapon->ItemState = EItemState::EIS_Equiped;
			CharacterState = ECharacterState::ECS_EquippedOneHandedWeapon;
			EquippedWeapon = OverlappingWeapon;
			OverlappingItem = nullptr;
		}
		else {
			if (CanDisarm()) {
				PlayEquipMontage(FName("Unequip"));
				CharacterState = ECharacterState::ECS_Unequipped;
			}
			else if (CanArm()) {
				PlayEquipMontage(FName("Equip"));
				CharacterState = ECharacterState::ECS_EquippedOneHandedWeapon;
			}
		}
	}
}

void ASlashCharacter::Drop(const FInputActionValue& Value)
{
	const float bValue = Value.Get<bool>();

	if (Controller && bValue) {
		if (EquippedWeapon && EquippedWeapon->ItemState != EItemState::EIS_Equipping && CanDisarm()) {
			EquippedWeapon->Drop(this->GetActorLocation(), this->GetActorForwardVector());
			CharacterState = ECharacterState::ECS_Unequipped;
			EquippedWeapon = nullptr;
		}
	}
}

bool ASlashCharacter::CanDisarm() {
	return (EquippedWeapon && 
		EquippedWeapon->ItemState == EItemState::EIS_Equiped &&
		EquippedWeapon->GetWeaponActionState() == EActionState::EAS_Unoccupied &&
		CharacterState != ECharacterState::ECS_Unequipped);
}

bool ASlashCharacter::CanArm() {
	return (EquippedWeapon && 
		EquippedWeapon->ItemState == EItemState::EIS_Equiped &&
		EquippedWeapon->GetWeaponActionState() == EActionState::EAS_Unoccupied &&
		CharacterState == ECharacterState::ECS_Unequipped);
}

void ASlashCharacter::Disarm()
{
	if (EquippedWeapon) {
		EquippedWeapon->ItemState = EItemState::EIS_Equiped;
		EquippedWeapon->AttachMeshToSocket(GetMesh(), FName("WeaponSocket"));
	}
}

void ASlashCharacter::Arm()
{
	if (EquippedWeapon) EquippedWeapon->AttachMeshToSocket(GetMesh(), FName("RightHandSocket"));
	
}
void ASlashCharacter::ArmEnd()
{
	if (EquippedWeapon) EquippedWeapon->ItemState = EItemState::EIS_Equiped;
}

void ASlashCharacter::PlayEquipMontage(const FName Selection) {
	TObjectPtr<UAnimInstance> AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && EquipMontage) {
		EquippedWeapon->ItemState = EItemState::EIS_Equipping;
		AnimInstance->Montage_Play(EquipMontage);
		AnimInstance->Montage_JumpToSection(Selection, EquipMontage);
	}

}

/**
*	CHARACTER COMBAT
*	Since Combat depands on Weapon selection
*	Character class call Weapon class for Attack options
*	Each Weapon have their own unique combat options
*/

void ASlashCharacter::Attack(const FInputActionValue& Value) {
	EquippedWeapon->Attack(Value);
}


void ASlashCharacter::AttackEnd()
{
	if (EquippedWeapon) EquippedWeapon->SetWeaponActionState(EActionState::EAS_Unoccupied);
}


void ASlashCharacter::SetWeaponCollision(ECollisionEnabled::Type CollisionEnabled)
{
	if (EquippedWeapon) EquippedWeapon->SetWeaponCollision(CollisionEnabled);
}


/**
*	CHARACTER DEFAULTS
*/

void ASlashCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}


void ASlashCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (TObjectPtr<UEnhancedInputComponent> EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent)) {
		EnhancedInputComponent->BindAction(MovementAction, ETriggerEvent::Triggered, this, &ASlashCharacter::Move);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ASlashCharacter::Look);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Triggered, this, &ASlashCharacter::Jump);
		EnhancedInputComponent->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &ASlashCharacter::Zoom);
		EnhancedInputComponent->BindAction(EquipAction, ETriggerEvent::Triggered, this, &ASlashCharacter::Equip);
		EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Triggered, this, &ASlashCharacter::Attack);
		EnhancedInputComponent->BindAction(DropAction, ETriggerEvent::Triggered, this, &ASlashCharacter::Drop);
	}

}